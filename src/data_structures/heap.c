#include "heap.h"
#include <stdlib.h>

typedef struct heap_element {
    void *value;
    int priority;
} heap_element;

struct heap_s {
    size_t size, capacity;
    heap_element *arr;
};

static inline size_t parent(size_t i) { return (i - 1u) >> 1; }
static inline size_t left(size_t i)   { return (i << 1) + 1u; }
static inline size_t right(size_t i)  { return (i << 1) + 2u; }

static inline int elem_cmp(const heap_element* a, const heap_element* b) {
    return b->priority - a->priority;
}

static inline void swap_elem(heap_element* a, heap_element* b) {
    heap_element tmp = *a; *a = *b; *b = tmp;
}

static void sift_up(heap h, size_t i) {
    while (i > 0) {
        size_t p = parent(i);
        if (elem_cmp(&h->arr[i], &h->arr[p]) < 0) {
            swap_elem(&h->arr[i], &h->arr[p]);
            i = p;
        } else {
            break;
        }
    }
}

static void sift_down(heap h, size_t i) {
    while (1) {
        size_t l = left(i);
        size_t r = right(i);
        size_t smallest = i;

        if (l < h->size && elem_cmp(&h->arr[l], &h->arr[smallest]) < 0) {
            smallest = l;
        }
        if (r < h->size && elem_cmp(&h->arr[r], &h->arr[smallest]) < 0) {
            smallest = r;
        }
        if (smallest == i) break;

        swap_elem(&h->arr[i], &h->arr[smallest]);
        i = smallest;
    }
}

static int ensure_capacity(heap h, size_t need) {
    if (need <= h->capacity) return 0;
    size_t new_cap = h->capacity ? (h->capacity * 2u) : 16u;
    if (new_cap < need) new_cap = need;

    heap_element* p = (heap_element*) realloc(h->arr, new_cap * sizeof(heap_element));
    if (!p) return -1;

    h->arr = p;
    h->capacity = new_cap;
    return 0;
}

heap heap_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 16;
    heap h = (heap) malloc(sizeof(struct heap_s));
    if (h == NULL) {
        return NULL;
    }
    h->arr = (heap_element*) malloc(initial_capacity * sizeof(heap_element));
    if (h->arr == NULL) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = initial_capacity;
    return h;
}

size_t heap_size(const heap h) {
    return h ? h->size : 0u;
}

int heap_is_empty(const heap h) {
    return (!h || h->size == 0) ? 1 : 0;
}

int heap_insert(heap h, void* value, int priority) {
    if (ensure_capacity(h, h->size + 1u) != 0) return -1;

    size_t i = h->size++;
    h->arr[i].value = value;
    h->arr[i].priority = priority;

    sift_up(h, i);
    return 0;
}

int heap_pop(heap h, void **out_value, int *out_priority) {
    if (h->size == 0) return 1;

    if (out_value) *out_value = h->arr[0].value;
    if (out_priority) *out_priority = h->arr[0].priority;

    h->arr[0] = h->arr[h->size - 1u];
    h->size -= 1u;

    if (h->size > 0) sift_down(h, 0u);
    return 0;
}

int heap_peek(const heap h, void** out_value, int* out_priority) {
    if (h->size == 0) return 0;
    if (out_value) *out_value = h->arr[0].value;
    if (out_priority) *out_priority = h->arr[0].priority;
    return 0;
}

void heap_destroy(heap h) {
    if (h == NULL) return;
    free(h->arr);
    free(h);
}
