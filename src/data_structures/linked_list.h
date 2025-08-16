#ifndef _H_LINKED_LIST_H_
#define _H_LINKED_LIST_H_

#include <stddef.h>
#include "data_structures.h"

typedef struct linked_list_s* linked_list;

linked_list linked_list_create();
int linked_list_pushfront(linked_list, const void *element);
void *linked_list_popfront(linked_list);
size_t linked_list_foreach(linked_list, iteration_result (*callback) (const void* value));
size_t linked_list_foreach_args(linked_list, iteration_result (*callback) (const void* value, void* args), void* args);
void linked_list_destroy(linked_list);

#endif
