/**
 * SPDX-FileCopyrightText: Copyright 2010-present Greg Hurrell and contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "score.h"

#include <stddef.h> /* for size_t */
#ifdef DEBUG_SCORING
#include <stdio.h> /* for fprintf, stdout */
#endif
#include <stdlib.h> /* for NULL */

// Scoring tunables.
//
// The per-character "factor ladder" (see `factor_for()`) is inherited verbatim
// from the historical scorer, so all of the boundary-preference behaviour ("/"
// beats "_" beats "." etc.) is unchanged. On top of it, a character that matches
// immediately after the previous match (a "consecutive" character, ie. part of a
// contiguous run) is worth `BONUS_CONSECUTIVE`. Because that exceeds every
// boundary bonus, a contiguous substring match dominates a scattered one (eg.
// "com" prefers "command/x" over "c/o/m/x", and "ab" prefers a consecutive "ab"
// over a split match), which also biases matches towards whole filename
// components without needing a separate basename term.
//
// The bonus is a constant (rather than growing with run length) on purpose: it
// keeps the score of a match a function of the matched positions alone, with no
// dependence on the order in which characters were placed. That is what lets the
// dynamic program below find the true maximum with a single value per cell.
#define BONUS_CAMEL 0.8f // camelCase hump.
#define BONUS_SLASH 0.9f // Character follows a "/".
#define BONUS_WORD 0.8f // Character follows "-", "_", " ", or a digit.
#define BONUS_DOT 0.7f // Character follows ".".
#define BONUS_CONSECUTIVE \
    1.3f // Character immediately follows the previous match.

static inline char downcase(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c | 0x20) : c;
}

// The historical per-character factor: how much a match at `haystack_idx` is
// worth relative to `max_score_per_char`, given that the previous matched
// character was at `last_idx`. A distance of 0 or 1 (ie. the very first
// character, or a character immediately following the previous match) is worth
// the full amount; otherwise the factor depends on the character preceding the
// match (a boundary) or decays with the size of the gap.
static inline float factor_for(const char *haystack, size_t j, size_t last_idx) {
    size_t distance = j - last_idx;
    if (distance <= 1) {
        return 1.0f;
    }
    char last = haystack[j - 1];
    char d = haystack[j];

    // Ordered with most common branches first.
    if (last >= 'a' && last <= 'z') {
        if (d >= 'A' && d <= 'Z') {
            return BONUS_CAMEL;
        }
        return (1.0f / distance) * 0.75f;
    } else if (last == '/') {
        return BONUS_SLASH;
    } else if (last == '-' || last == '_' || last == ' ' || (last >= '0' && last <= '9')) {
        return BONUS_WORD;
    } else if (last == '.') {
        return BONUS_DOT;
    }
    // No "special" char behind this one, so the factor diminishes as the gap
    // grows.
    return (1.0f / distance) * 0.75f;
}

float commandt_score_upper_bound(size_t needle_length, size_t candidate_length) {
    if (needle_length == 0 || candidate_length == 0) {
        return 1.0f;
    }
    float base = (1.0f / candidate_length + 1.0f / needle_length) / 2.0f;

    // Best case: every needle character forms a single contiguous run. The first
    // character is worth at most 1.0; each subsequent one at most
    // `BONUS_CONSECUTIVE` (which exceeds every boundary bonus). This is an
    // admissible upper bound: no alignment can score higher, so it is safe to
    // use for pruning.
    return base * (1.0f + BONUS_CONSECUTIVE * (float)(needle_length - 1));
}

float commandt_score(haystack_t *haystack, matcher_t *matcher, bool ignore_case) {
    const char *haystack_p = haystack->candidate->contents;
    size_t haystack_len = haystack->candidate->length;
    const char *needle_p = matcher->needle;
    size_t needle_length = matcher->needle_length;
    bool always_show_dot_files = matcher->always_show_dot_files;
    bool never_show_dot_files = matcher->never_show_dot_files;
    bool compute_bitmasks = haystack->bitmask == UNSET_BITMASK;

    // Special case for zero-length search string.
    if (needle_length == 0) {
        // Filter out dot files.
        if (never_show_dot_files || !always_show_dot_files) {
            for (size_t i = 0; i < haystack_len; i++) {
                char c = haystack_p[i];
                if (c == '.' && (i == 0 || haystack_p[i - 1] == '/')) {
                    return -1.0f;
                }
            }
        }
        return 1.0f;
    }

    if (haystack->bitmask != UNSET_BITMASK) {
        if ((matcher->needle_bitmask & haystack->bitmask) !=
            matcher->needle_bitmask) {
            return 0.0f;
        }
    }

    // Pre-scan string:
    // - Bail if it can't match at all.
    // - Record rightmost match for each character (prune search space).
    // - Record bitmask for haystack to speed up future searches.
    size_t rightmost_match_p[needle_length];
    size_t needle_idx = needle_length - 1;
    size_t haystack_idx = haystack_len ? haystack_len - 1 : 0;
    long mask = 0;
    bool found_needle = false;
    if (haystack_len) {
        while (haystack_idx >= needle_idx) {
            char c = haystack_p[haystack_idx];
            char lower = c >= 'A' && c <= 'Z' ? c | 0x20 : c;
            if (ignore_case) {
                c = lower;
            }
            if (compute_bitmasks) {
                mask |= (1 << (lower - 'a'));
            }

            char d = needle_p[needle_idx];
            if (c == d) {
                rightmost_match_p[needle_idx] = haystack_idx;
                if (needle_idx == 0) {
                    found_needle = true;
                    break;
                } else {
                    needle_idx--;
                }
            }

            if (haystack_idx == 0) {
                break;
            } else {
                haystack_idx--;
            }
        }
    }
    if (compute_bitmasks) {
        if (haystack_len) {
            // In case we broke out of the loop early, compute rest of mask.
            for (size_t i = 0; i <= haystack_idx; i++) {
                char c = haystack_p[i];
                char lower = c >= 'A' && c <= 'Z' ? c | 0x20 : c;
                mask |= (1 << (lower - 'a'));
            }
        }
        haystack->bitmask = mask;
    }
    if (!found_needle) {
        return 0.0f;
    }

    // Only positions in `[0, limit)` can participate in a match.
    size_t limit = rightmost_match_p[needle_length - 1] + 1;
    float base = (1.0f / haystack_len + 1.0f / needle_length) / 2.0f;

    // Dot-file handling. A "forbidden" dot is one that begins a hidden path
    // component (a "." at index 0 or immediately after a "/"). Depending on the
    // configured policy:
    //
    // - `always_show_dot_files`: no constraint.
    // - `never_show_dot_files`: any forbidden dot in range excludes the
    //   candidate outright.
    // - default: a forbidden dot must be matched *explicitly* by a "." in the
    //   needle (ie. no match may "skip over" it). This is enforced in the DP by
    //   forbidding transitions whose skipped span contains a forbidden dot.
    bool dot_gate = !always_show_dot_files;
    size_t forbidden_prefix[limit + 1];
    if (dot_gate) {
        size_t acc = 0;
        forbidden_prefix[0] = 0;
        for (size_t k = 0; k < limit; k++) {
            if (haystack_p[k] == '.' && (k == 0 || haystack_p[k - 1] == '/')) {
                acc++;
            }
            forbidden_prefix[k + 1] = acc;
        }
        if (never_show_dot_files && acc > 0) {
            return 0.0f;
        }
    }
    bool enforce_dots = dot_gate && !never_show_dot_files;

    // Forward dynamic program. `D[i][j]` is the best score for matching
    // `needle[0..i]` with `needle[i]` anchored at haystack position `j`. We keep
    // only the current and previous rows, each stored sparsely as a list of the
    // reachable positions plus a position-indexed score array. Because a
    // character's contribution depends only on its position and the previous
    // match's position (never on run length), a single score per cell suffices
    // to find the global maximum.
    float score_a[limit], score_b[limit];
    size_t list_a[limit], list_b[limit];
    float *prev_score = score_b, *cur_score = score_a;
    size_t *prev_list = list_b, *cur_list = list_a;
    size_t prev_count = 0, cur_count = 0;

    // Row 0: place needle[0] at each matching position.
    char needle_0 = needle_p[0];
    for (size_t j = 0; j <= rightmost_match_p[0]; j++) {
        char d = haystack_p[j];
        char d_cmp = ignore_case ? downcase(d) : d;
        if (d_cmp != needle_0) {
            continue;
        }
        // The span before the first match must not skip a forbidden dot.
        if (enforce_dots && forbidden_prefix[j] > 0) {
            continue;
        }
        float q = factor_for(haystack_p, j, 0);
        cur_score[j] = base * q;
        cur_list[cur_count++] = j;
    }
    if (cur_count == 0) {
        return 0.0f;
    }

    // Rows 1..needle_length-1.
    for (size_t i = 1; i < needle_length; i++) {
        // Swap: previous row becomes what we just computed.
        prev_count = cur_count;
        float *ts = prev_score;
        prev_score = cur_score;
        cur_score = ts;
        size_t *tl = prev_list;
        prev_list = cur_list;
        cur_list = tl;
        cur_count = 0;

        char needle_i = needle_p[i];
        for (size_t j = i; j <= rightmost_match_p[i]; j++) {
            char d = haystack_p[j];
            char d_cmp = ignore_case ? downcase(d) : d;
            if (d_cmp != needle_i) {
                continue;
            }

            float best = 0.0f;
            for (size_t t = 0; t < prev_count; t++) {
                size_t p = prev_list[t];
                if (p >= j) {
                    continue;
                }
                // The skipped span (p, j) must not contain a forbidden dot.
                if (enforce_dots &&
                    forbidden_prefix[j] - forbidden_prefix[p + 1] > 0) {
                    continue;
                }

                // A character immediately following the previous match is worth
                // `BONUS_CONSECUTIVE`; otherwise it falls to the boundary/gap
                // ladder. (For distance 1 the ladder is 1.0, always below the
                // consecutive bonus, so the `max` is really just documentation.)
                float q;
                if (j == p + 1) {
                    q = BONUS_CONSECUTIVE;
                } else {
                    q = factor_for(haystack_p, j, p);
                }
                float total = prev_score[p] + base * q;
                if (total > best) {
                    best = total;
                }
            }

            if (best > 0.0f) {
                cur_score[j] = best;
                cur_list[cur_count++] = j;
            }
        }

        if (cur_count == 0) {
            // No way to match needle[0..i]; nothing can extend it either.
            return 0.0f;
        }
    }

    // The answer is the best score in the final row.
    float score = 0.0f;
    for (size_t t = 0; t < cur_count; t++) {
        float s = cur_score[cur_list[t]];
        if (s > score) {
            score = s;
        }
    }

#ifdef DEBUG_SCORING
    fprintf(stdout, "needle='%.*s' ", (int)needle_length, needle_p);
    fprintf(stdout, "haystack='%.*s' ", (int)haystack_len, haystack_p);
    fprintf(stdout, "score=%f\n", score);
#endif

    return score;
}
