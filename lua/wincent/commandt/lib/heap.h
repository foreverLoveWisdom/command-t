/**
 * SPDX-FileCopyrightText: Copyright 2016-present Greg Hurrell and contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file
 *
 * A fixed size min-heap, specialized for Command-T.
 */

#ifndef HEAP_H
#define HEAP_H

#include "commandt.h" /* for haystack_t */

// Define short names for convenience, but all external symbols need prefixes.
#define heap_extract commandt_heap_extract
#define heap_free commandt_heap_free
#define heap_insert commandt_heap_insert
#define heap_new commandt_heap_new

typedef struct {
    unsigned count;
    unsigned capacity;
    haystack_t **entries;
} heap_t;

#define HEAP_PEEK(heap) (heap->entries[0])

/**
 * Extracts the minimum value from `heap`.
 */
haystack_t *heap_extract(heap_t *heap);

/**
 * Frees a previously created heap.
 */
void heap_free(heap_t *heap);

/**
 * Inserts `value` into `heap`.
 */
void heap_insert(heap_t *heap, haystack_t *value);

/**
 * Returns a new heap.
 */
heap_t *heap_new(unsigned capacity);

#endif
