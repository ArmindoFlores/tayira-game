#include "linked_list.h"
#include <stdlib.h>

struct linked_list_element_s {
    void *value;
    struct linked_list_element_s *next;
};

struct linked_list_s {
    struct linked_list_element_s *head;
};

struct f_cb_s {
    iteration_result (*f) (const void*);
};

linked_list linked_list_create() {
    linked_list ll = (linked_list) malloc(sizeof(struct linked_list_s));
    if (ll == NULL) {
        return NULL;
    }
    ll->head = NULL;
    return ll;
}

int linked_list_pushfront(linked_list ll, const void *element) {
    struct linked_list_element_s *new_element = (struct linked_list_element_s *) malloc(sizeof(struct linked_list_element_s));
    if (new_element == NULL) {
        return 1;
    }

    new_element->value = (void *) element;

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

size_t linked_list_foreach_args(linked_list ll, iteration_result (*callback) (const void* value, void* args), void* args) {
    size_t visited = 0;
    for (struct linked_list_element_s *cur = ll->head; cur != NULL; cur = cur->next) {
        iteration_result result = callback(cur->value, args);
        visited++;
        if (result == ITERATION_BREAK) break;
    }
    return visited;
}

static iteration_result foreach_noargs_helper(const void* value, void* args) {
    struct f_cb_s *f_cb = (struct f_cb_s *) args;
    return f_cb->f(value);
}

size_t linked_list_foreach(linked_list ll, iteration_result (*callback) (const void* value)) {
    struct f_cb_s f_cb = { .f = callback };
    return linked_list_foreach_args(ll, foreach_noargs_helper, &f_cb);
}

void linked_list_destroy(linked_list ll) {
    if (ll == NULL) return;
    free(ll);
}
