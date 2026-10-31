#ifndef VEC_H
#define VEC_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Minimal growable array (vector) for internal use.
 *
 * The API is intentionally small: it manages capacity and count, but the
 * caller owns the element type and any per-element allocation/cleanup.
 *
 * Usage pattern:
 *
 *     struct my_item *items = NULL;
 *     size_t count = 0, capacity = 0;
 *
 *     // push
 *     if (!vec_push(&items, &count, &capacity, sizeof(*items)))
 *         return PL_ERR_MEMORY;
 *     items[count - 1] = ...;  // initialize the new slot
 *
 *     // iterate
 *     for (size_t i = 0; i < count; i++) { ... }
 *
 *     // free
 *     for (size_t i = 0; i < count; i++) { free(items[i].owned_field); }
 *     free(items);
 */

/* Ensure `*cap` is at least `min` (rounded up to a power of 2 >= 8).
 * Returns 0 on success, -1 on allocation failure.  On failure the
 * original pointer is unchanged and must be freed by the caller. */
static inline int vec_grow(void **ptr, size_t *cap, size_t min, size_t elem_size)
{
    if (*cap >= min)
        return 0;

    size_t new_cap = (*cap == 0) ? 8 : *cap;
    while (new_cap < min)
        new_cap *= 2;

    void *grown = realloc(*ptr, new_cap * elem_size);
    if (grown == NULL)
        return -1;

    *ptr = grown;
    *cap = new_cap;
    return 0;
}

/* Append one zero-initialized element to the vector.
 * Returns 0 on success, -1 on allocation failure.  On success, the new
 * element is at index `*count` (before increment) and `*count` is
 * incremented.  The new element's memory is zeroed. */
static inline int vec_push_zero(void **ptr, size_t *count, size_t *cap, size_t elem_size)
{
    if (vec_grow(ptr, cap, *count + 1, elem_size) != 0)
        return -1;

    char *base = *ptr;
    memset(base + *count * elem_size, 0, elem_size);
    (*count)++;
    return 0;
}

/* Append one element by copying `elem` into the vector.
 * Returns 0 on success, -1 on allocation failure. */
static inline int vec_push_copy(void **ptr, size_t *count, size_t *cap,
                                const void *elem, size_t elem_size)
{
    if (vec_grow(ptr, cap, *count + 1, elem_size) != 0)
        return -1;

    char *base = *ptr;
    memcpy(base + *count * elem_size, elem, elem_size);
    (*count)++;
    return 0;
}

/* Remove the element at `index` by shifting later elements down.
 * Does not shrink capacity.  Caller must clean up the removed element
 * if it owns resources (do that BEFORE calling this). */
static inline void vec_remove_at(void *ptr, size_t *count, size_t index, size_t elem_size)
{
    char *base = ptr;
    size_t last = *count - 1;
    if (index < last)
        memmove(base + index * elem_size, base + (index + 1) * elem_size,
                (last - index) * elem_size);
    (*count)--;
}

/* Free the vector's storage and zero the count/capacity.
 * Does NOT free individual elements; caller must do that first. */
static inline void vec_free(void **ptr, size_t *count, size_t *cap)
{
    free(*ptr);
    *ptr = NULL;
    *count = 0;
    *cap = 0;
}

#endif /* VEC_H */