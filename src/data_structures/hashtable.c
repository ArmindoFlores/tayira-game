#include "hashtable.h"
#include <stdlib.h>
#include <string.h>

static const uint64_t FNV_OFFSET = UINT64_C(14695981039346656037);
static const uint64_t FNV_PRIME  = UINT64_C(1099511628211);
static const double MAX_LOAD = 0.75;

struct hashtable_entry_list_element_s {
    char *key;
    void *element;
    struct hashtable_entry_list_element_s *next;
};

struct hashtable_s {
    size_t capacity;
    size_t size;
    struct hashtable_entry_list_element_s **entries;
};

struct f_cb_s {
    iteration_result (*f) (const hashtable_entry*);
};

hashtable hashtable_create() {
    hashtable h = (hashtable) malloc(sizeof(struct hashtable_s));
    if (h == NULL) {
        hashtable_destroy(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = 16;
    h->entries = (struct hashtable_entry_list_element_s **) calloc(h->capacity, sizeof(struct hashtable_entry_list_element_s *));
    if (h->entries == NULL) {
        hashtable_destroy(h);
        return NULL;
    }
    return h;
}

static uint64_t hash_key(const char *key) {
    uint64_t hash = FNV_OFFSET;
    for (const unsigned char *p = (const unsigned char*)key; *p; ++p) {
        hash ^= *p;
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint64_t index_from_hash(hashtable h, uint64_t hash) {
    return hash % h->capacity;
}

static struct hashtable_entry_list_element_s *get_entry(hashtable h, const char *key) {
    uint64_t hash = hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *ptr = h->entries[index];
    while (ptr != NULL) {
        if (strcmp(ptr->key, key) == 0) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

static int hashtable_resize(hashtable h) {
    size_t old_capacity = h->capacity;
    size_t new_capacity = (old_capacity > 0) ? (old_capacity * 2) : 16;

    struct hashtable_entry_list_element_s **new_entries = (struct hashtable_entry_list_element_s **) calloc(new_capacity, sizeof(struct hashtable_entry_list_element_s *));
    if (new_entries == NULL) {
        return 1;
    }

    for (size_t i = 0; i < old_capacity; i++) {
        struct hashtable_entry_list_element_s *node = h->entries[i];
        while (node) {
            struct hashtable_entry_list_element_s *next = node->next;

            size_t hash = hash_key(node->key);
            size_t index = hash % new_capacity;

            node->next = new_entries[index];
            new_entries[index] = node;

            node = next;
        }
    }

    free(h->entries);
    h->entries = new_entries;
    h->capacity = new_capacity;

    return 0;
}

void *hashtable_get(hashtable h, const char* key) {
    struct hashtable_entry_list_element_s *result = get_entry(h, key);
    if (result == NULL) {
        return NULL;
    }
    return result->element;
}

int hashtable_has(hashtable h, const char* key) {
    return hashtable_get(h, key) != NULL;
}

int hashtable_set(hashtable h, const char* key, void* element) {
    struct hashtable_entry_list_element_s *existing = get_entry(h, key);
    if (existing != NULL) {
        existing->element = element;
        return 0;
    }

    if ((double)(h->size + 1) / (double)h->capacity > MAX_LOAD) {
        hashtable_resize(h); // This might fail
    }

    uint64_t hash = hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *new_node = (struct hashtable_entry_list_element_s *) malloc(sizeof(struct hashtable_entry_list_element_s));
    if (new_node == NULL) {
        return 1;
    }
    
    size_t key_size = strlen(key);
    new_node->key = (char*) malloc(key_size + 1);

    if (new_node->key == NULL) {
        free(new_node);
        return 1;
    }

    memcpy(new_node->key, key, key_size);
    new_node->key[key_size] = '\0';
    new_node->element = element;
    
    struct hashtable_entry_list_element_s *head = h->entries[index];
    h->entries[index] = new_node;
    new_node->next = head;
    h->size++;

    return 0;
}

int hashtable_delete(hashtable h, const char* key) {
    uint64_t hash = hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *ptr = h->entries[index], *prev_ptr = NULL;
    while (ptr != NULL) {
        if (strcmp(ptr->key, key) == 0) {
            if (prev_ptr != NULL) {
                prev_ptr->next = ptr->next;
            }
            else {
                h->entries[index] = ptr->next;
            }
            free(ptr->key);
            free(ptr);
            h->size--;
            return 0;
        }
        prev_ptr = ptr;
        ptr = ptr->next;
    }
    return 1;
}

size_t hashtable_foreach_args(hashtable t, iteration_result (*callback) (const hashtable_entry*, void* args), void* args) {
    size_t visited = 0;

    for (size_t i = 0; i < t->capacity; ++i) {
        struct hashtable_entry_list_element_s *node = t->entries[i];
        while (node) {
            struct hashtable_entry_list_element_s *next = node->next;

            hashtable_entry entry = { .key = node->key, .value = node->element };
            ++visited;

            if (callback(&entry, args) == ITERATION_BREAK) {
                return visited;
            }
            node = next;
        }
    }
    return visited;
}

static iteration_result foreach_noargs_helper(const hashtable_entry *entry, void* args) {
    struct f_cb_s *f_cb = (struct f_cb_s *) args;
    return f_cb->f(entry);
}

size_t hashtable_foreach(hashtable t, iteration_result (*callback) (const hashtable_entry*)) {
    struct f_cb_s f_cb = { .f = callback };
    return hashtable_foreach_args(t, foreach_noargs_helper, &f_cb);
}

void hashtable_destroy(hashtable h) {
    if (!h) return;
    if (h->entries) {
        for (size_t i = 0; i < h->capacity; i++) {
            struct hashtable_entry_list_element_s *ptr = h->entries[i];
            while (ptr) {
                struct hashtable_entry_list_element_s *next = ptr->next;
                free(ptr->key);
                free(ptr);
                ptr = next;
            }
        }
        free(h->entries);
    }
    free(h);
}
