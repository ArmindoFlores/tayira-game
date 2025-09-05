#ifndef _H_LINKED_LIST_H_
#define _H_LINKED_LIST_H_

#include <stddef.h>
#include "data_structures.h"

typedef struct linked_list_s* linked_list;

linked_list linked_list_create(
    size_t element_size,
    free_function free_element,
    copy_function copy_element
);
linked_list linked_list_create_owned(free_function free_element);
linked_list linked_list_create_borrowed();
linked_list linked_list_create_trivial(size_t element_size, copy_function copy_element);

int linked_list_pushfront(linked_list, const void *element);
void *linked_list_popfront(linked_list);
void linked_list_remove_if(linked_list, predicate_function, void *args);
size_t linked_list_foreach(linked_list, iteration_result (*callback) (void* value));
size_t linked_list_foreach_args(linked_list, iteration_result (*callback) (void* value, void* args), void* args);
size_t linked_list_size(linked_list);
void linked_list_destroy(linked_list);

#endif
