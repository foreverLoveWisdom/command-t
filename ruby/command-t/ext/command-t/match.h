/**
 * SPDX-FileCopyrightText: Copyright 2010-present Greg Hurrell and contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ruby.h>

#define UNSET_BITMASK (-1)

// Struct for representing an individual match.
typedef struct {
    VALUE path;
    long bitmask;
    float score;
} match_t;

extern float calculate_match(
    VALUE str,
    VALUE needle,
    VALUE case_sensitive,
    VALUE always_show_dot_files,
    VALUE never_show_dot_files,
    VALUE recurse,
    long needle_bitmask,
    long *haystack_bitmask
);
