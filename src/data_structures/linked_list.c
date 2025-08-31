#include "linked_list.h"
#include <stdlib.h>
#include <stdint.h>

struct linked_list_element_s {
    void *value;
    struct linked_list_element_s *next;
};

struct linked_list_s {
    size_t size;
    struct linked_list_element_s *head;
};

struct f_cb_s {
    iteration_result (*f) (void*);
};

linked_list linked_list_create() {
    linked_list ll = (linked_list) malloc(sizeof(struct linked_list_s));
    if (ll == NULL) {
        return NULL;
    }
    ll->head = NULL;
    ll->size = 0;
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
    ll->size++;
    return 0;
}

void *linked_list_popfront(linked_list ll) {
    if (ll->head == NULL) {
        return NULL;
    }
    ll->size--;
    void *result = ll->head->value;
    struct linked_list_element_s *prev_head = ll->head;
    ll->head = ll->head->next;
    free(prev_head);
    return result;
}

void linked_list_remove_if(linked_list ll, predicate_function f, void *args) {
    if (ll->head == NULL) {
        return;
    }
    
    struct linked_list_element_s *cur = ll->head, *prev = NULL;
    while (cur != NULL) {
        if (f(cur->value, args)) {
            if (prev == NULL) {
                ll->head = cur->next;
            }
            else {
                prev->next = cur->next;
            }
            struct linked_list_element_s *tmp = cur->next;
            free(cur);
            cur = tmp;
        }
        else {
            prev = cur;
            cur = cur->next;
        }
    }
}

size_t linked_list_foreach_args(linked_list ll, iteration_result (*callback) (void* value, void* args), void* args) {
    size_t visited = 0;
    for (struct linked_list_element_s *cur = ll->head; cur != NULL; cur = cur->next) {
        iteration_result result = callback(cur->value, args);
        visited++;
        if (result == ITERATION_BREAK) return SIZE_MAX;
    }
    return visited;
}

static iteration_result foreach_noargs_helper(void* value, void* args) {
    struct f_cb_s *f_cb = (struct f_cb_s *) args;
    return f_cb->f(value);
}

size_t linked_list_foreach(linked_list ll, iteration_result (*callback) (void* value)) {
    struct f_cb_s f_cb = { .f = callback };
    return linked_list_foreach_args(ll, foreach_noargs_helper, &f_cb);
}

size_t linked_list_size(linked_list ll) {
    return ll->size;
}

void linked_list_destroy(linked_list ll) {
    if (ll == NULL) return;
    struct linked_list_element_s *cur = ll->head;
    while (cur != NULL) {
        struct linked_list_element_s *next = cur->next;
        free(cur);
        cur = next;
    }
    free(ll);
}
