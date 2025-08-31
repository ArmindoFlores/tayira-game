#ifndef _H_HEAP_H_
#define _H_HEAP_H_

#include "data_structures.h"
#include <stddef.h>

/**
 * This type represents an opaque pointer to a heap.
 */
typedef struct heap_s *heap;

/**
 * This function creates a new heap with the specified capacity. The 
 * returned object must be destroyed using `heap_destroy(...)`.
 * 
 * @param initial_capacity the heap will store at most this number of 
 * items before calling `realloc()`
 * 
 * @return the heap pointer or NULL if this operation failed.
 */
heap heap_create(size_t initial_capacity);

/**
 * This function adds a new object into the a heap with a specified priority.
 * 
 * @param h the heap
 * @param value the value to be inserted; it is not owned by the heap and must
 * be freed by the user (if heap allocated)
 * @param priority the priority associated with this item
 * 
 * @return 0 if successful, 1 otherwise
 */
int heap_insert(heap h, void* value, int priority);

/**
 * This function returns the number of elements currently in the heap.
 * 
 * @param h the heap
 * 
 * @return the number of elements in the heap
 */
size_t heap_size(const heap h);

/**
 * This function checks whether a heap is empty
 * 
 * @param h the heap
 * 
 * @return 1 if the heap is empty, 0 otherwise
 */
int heap_is_empty(const heap h);

/**
 * This function removes the highest priority object from the heap and
 * retrieves its value and priority.
 * 
 * @param h the heap
 * @param out_value the value removed from the heap will be stored in 
 * the address pointed to by this pointer
 * @param out_priority the priority of the removed item will be stored
 * in the address pointed to by this pointer
 * 
 * @return 0 if successful, 1 otherwise
 */
int heap_pop(heap h, void **out_value, int *out_priority);

/**
 * This function retrieves the value and priority of the highest priority
 * object in the heap.
 * 
 * @param h the heap
 * @param out_value the value of the retrieved item will be stored in 
 * the address pointed to by this pointer
 * @param out_priority the priority of the retrieved item will be stored
 * in the address pointed to by this pointer
 */
int heap_peek(const heap h, void** out_value, int* out_priority);

// FIXME: refine
// Returns true if any of the elements satisfy the predicate
int heap_any(const heap h, predicate_function, void *args);

/**
 * This function frees the resources taken up by a heap. It must be called
 * for each heap created with `heap_create()`.
 * 
 * @param h the heap
 */
void heap_destroy(heap h);

#endif
