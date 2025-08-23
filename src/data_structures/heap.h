#ifndef _H_HEAP_H_
#define _H_HEAP_H_

#include <stddef.h>

typedef struct heap_s *heap;

heap heap_create(size_t initial_capacity);
int heap_insert(heap, void* value, int priority);
size_t heap_size(const heap);
int heap_is_empty(const heap);
int heap_insert(heap, void* value, int priority);
int heap_pop(heap, void **out_value, int *out_priority);
int heap_peek(const heap, void** out_value, int* out_priority);
void heap_destroy(heap);

#endif
