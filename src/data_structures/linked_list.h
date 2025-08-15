#ifndef _H_LINKED_LIST_H_
#define _H_LINKED_LIST_H_

typedef struct linked_list_s* linked_list;
typedef struct linked_list_iterator_s* linked_list_iterator;

linked_list linked_list_create();
int linked_list_pushfront(linked_list, void *element);
void *linked_list_popfront(linked_list);
linked_list_iterator linked_list_begin_iter(linked_list);
void *linked_list_next_iter(linked_list_iterator);
void linked_list_end_iter(linked_list_iterator);
void linked_list_destroy(linked_list);

#endif