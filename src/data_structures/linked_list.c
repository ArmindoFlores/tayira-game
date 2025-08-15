#include "linked_list.h"
#include <stdlib.h>

struct linked_list_element_s {
    void *value;
    struct linked_list_element_s *next;
};

struct linked_list_s {
    struct linked_list_element_s *head;
};

struct linked_list_iterator_s {
    linked_list list;
    struct linked_list_element_s *current;
};

linked_list linked_list_create() {
    linked_list ll = (linked_list) malloc(sizeof(struct linked_list_s));
    if (ll == NULL) {
        return NULL;
    }
    ll->head = NULL;
    return ll;
}

int linked_list_pushfront(linked_list ll, void *element) {
    struct linked_list_element_s *new_element = (struct linked_list_element_s *) malloc(sizeof(struct linked_list_element_s));
    if (new_element == NULL) {
        return 1;
    }

    new_element->value = element;

    if (ll->head == NULL) {
        ll->head = new_element;
        new_element->next = NULL;
    }
    else {
        new_element->next = ll->head;
        ll->head = new_element;
    }

    return 0;
}

void *linked_list_popfront(linked_list ll) {
    if (ll->head == NULL) {
        return NULL;
    }
    void *result = ll->head->value;
    struct linked_list_element_s *prev_head = ll->head;
    ll->head = ll->head->next;
    free(prev_head);
    return result;
}

linked_list_iterator linked_list_begin_iter(linked_list ll) {
    linked_list_iterator it = (linked_list_iterator) malloc(sizeof (struct linked_list_iterator_s));
    if (it == NULL) {
        return NULL;
    }
    it->list = ll;
    it->current = ll->head;
    return it;
}

void *linked_list_next_iter(linked_list_iterator it) {
    if (it->current == NULL) {
        return NULL;
    }
    void *result = it->current->value;
    it->current = it->current->next;
    return result;
}

void linked_list_end_iter(linked_list_iterator it) {
    free(it);
}

void linked_list_destroy(linked_list ll) {
    if (ll == NULL) return;
    free(ll);
}
