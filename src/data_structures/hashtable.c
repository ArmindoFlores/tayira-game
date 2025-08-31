#include "hashtable.h"
#include <stdlib.h>
#include <string.h>

static const uint64_t FNV_OFFSET = UINT64_C(14695981039346656037);
static const uint64_t FNV_PRIME  = UINT64_C(1099511628211);
static const double MAX_LOAD = 0.75;

struct hashtable_entry_list_element_s {
    void *key;
    void *element;
    struct hashtable_entry_list_element_s *next;
};

struct hashtable_s {
    size_t capacity;
    size_t size, key_size, value_size;
    free_function free_key, free_value;
    copy_function copy_key, copy_value;
    compare_function compare_keys;
    hash_function hash_key;
    struct hashtable_entry_list_element_s **entries;
};

struct f_cb_s {
    iteration_result (*f) (const hashtable_entry*);
};

static uint64_t default_hash_key_impl(const void *key) {
    return (uint64_t) key;
}

uint64_t hashtable_hash_string(const void *key) {
    uint64_t hash = FNV_OFFSET;
    for (const unsigned char *p = (const unsigned char*)key; *p; ++p) {
        hash ^= *p;
        hash *= FNV_PRIME;
    }
    return hash;
}

static int default_compare_impl(const void *value1, const void *value2) {
    return value1 == value2;
}

int hashtable_compare_strings(const void *value1, const void* value2) {
    return strcmp((const char*) value1, (const char*) value2);
}

static void default_free_impl(void *) {
    return;
}

static void* default_copy_impl(const void *x, size_t) {
    return (void *) x;
}

void *hashtable_copy_trivial(const void *element, size_t element_size) {
    void *copy = malloc(element_size);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, element, element_size);
    return copy;
}

void *hashtable_copy_string(const void *element, size_t) {
    const char* str_element = (const char*) element;
    size_t str_len = strlen(str_element);
    char *copy = calloc(str_len + 1, sizeof(char));
    if (copy == NULL) {
        return NULL;
    }
    strcpy(copy, str_element);
    return copy;
}

static uint64_t index_from_hash(hashtable h, uint64_t hash) {
    return hash % h->capacity;
}

hashtable hashtable_create(
    size_t key_size,
    free_function free_key,
    copy_function copy_key,
    compare_function compare_keys,
    hash_function hash_key,
    size_t value_size,
    free_function free_value,
    copy_function copy_value
) {
    hashtable h = (hashtable) malloc(sizeof(struct hashtable_s));
    if (h == NULL) {
        hashtable_destroy(h);
        return NULL;
    }
    h->free_key = free_key == NULL ? default_free_impl : free_key;
    h->free_value = free_value == NULL ? default_free_impl : free_value;
    h->copy_value = copy_value == NULL ? default_copy_impl : copy_value;
    h->copy_key = copy_key == NULL ? default_copy_impl : copy_key;
    h->key_size = key_size;
    h->value_size = value_size;
    h->hash_key = hash_key == NULL ? default_hash_key_impl : hash_key;
    h->compare_keys = compare_keys == NULL ? default_compare_impl : compare_keys;
    h->size = 0;
    h->capacity = 16;
    h->entries = (struct hashtable_entry_list_element_s **) calloc(h->capacity, sizeof(struct hashtable_entry_list_element_s *));
    if (h->entries == NULL) {
        hashtable_destroy(h);
        return NULL;
    }
    return h;
}

hashtable hashtable_create_copied_string_key_borrowed_pointer_value() {
    return hashtable_create(
        sizeof(char*),
        free,
        hashtable_copy_string,
        hashtable_compare_strings,
        hashtable_hash_string,
        sizeof(void*),
        NULL,
        NULL
    );
}

hashtable hashtable_create_copied_string_key_owned_pointer_value(free_function free_value) {
    if (free_value == NULL) return NULL;
    return hashtable_create(
        sizeof(char*),
        free,
        hashtable_copy_string,
        hashtable_compare_strings,
        hashtable_hash_string,
        sizeof(void*),
        free_value,
        NULL
    );
}

hashtable hashtable_create_trivial_key_borrowed_pointer_value(size_t key_size, compare_function compare_keys, hash_function hash_key) {
    if (compare_keys == NULL || hash_key == NULL) return NULL;
    return hashtable_create(
        key_size,
        free,
        hashtable_copy_trivial,
        compare_keys,
        hash_key,
        sizeof(void*),
        NULL,
        NULL
    );
}

hashtable hashtable_create_trivial_key_owned_pointer_value(size_t key_size, compare_function compare_keys, hash_function hash_key, free_function free_value) {
    if (compare_keys == NULL || hash_key == NULL || free_value == NULL) return NULL;
    return hashtable_create(
        key_size,
        free,
        hashtable_copy_trivial,
        compare_keys,
        hash_key,
        sizeof(void*),
        free_value,
        NULL
    );
}

hashtable hashtable_create_trivial_key_trivial_value(size_t key_size, compare_function compare_keys, hash_function hash_key, size_t value_size) {
    if (compare_keys == NULL || hash_key == NULL) return NULL;
    return hashtable_create(
        key_size,
        free,
        hashtable_copy_trivial,
        compare_keys,
        hash_key,
        value_size,
        free,
        hashtable_copy_trivial
    );
}

static struct hashtable_entry_list_element_s *get_entry(hashtable h, const void *key) {
    uint64_t hash = h->hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *ptr = h->entries[index];
    while (ptr != NULL) {
        if (h->compare_keys(ptr->key, key) == 0) {
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

            size_t hash = h->hash_key(node->key);
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

void *hashtable_get(hashtable h, const void* key) {
    struct hashtable_entry_list_element_s *result = get_entry(h, key);
    if (result == NULL) {
        return NULL;
    }
    return result->element;
}

int hashtable_has(hashtable h, const void* key) {
    return hashtable_get(h, key) != NULL;
}

int hashtable_set(hashtable h, const void* key, void* element) {
    struct hashtable_entry_list_element_s *existing = get_entry(h, key);
    if (existing != NULL) {
        existing->element = element;
        return 0;
    }

    if ((double)(h->size + 1) / (double)h->capacity > MAX_LOAD) {
        hashtable_resize(h); // This might fail
    }

    uint64_t hash = h->hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *new_node = (struct hashtable_entry_list_element_s *) malloc(sizeof(struct hashtable_entry_list_element_s));
    if (new_node == NULL) {
        return 1;
    }
    
    new_node->key = h->copy_key(key, h->key_size);

    if (new_node->key == NULL) {
        free(new_node);
        return 1;
    }

    new_node->element = h->copy_value(element, h->value_size);
    
    if (new_node->element == NULL) {
        h->free_key(new_node->key);
        free(new_node);
    }

    struct hashtable_entry_list_element_s *head = h->entries[index];
    h->entries[index] = new_node;
    new_node->next = head;
    h->size++;

    return 0;
}

void* hashtable_pop(hashtable h, const void* key) {
    uint64_t hash = h->hash_key(key);
    uint64_t index = index_from_hash(h, hash);

    struct hashtable_entry_list_element_s *ptr = h->entries[index], *prev_ptr = NULL;
    while (ptr != NULL) {
        if (h->compare_keys(ptr->key, key) == 0) {
            if (prev_ptr != NULL) {
                prev_ptr->next = ptr->next;
            }
            else {
                h->entries[index] = ptr->next;
            }
            void *element = ptr->element;
            h->free_key(ptr->key);
            free(ptr);
            h->size--;
            return element;
        }
        prev_ptr = ptr;
        ptr = ptr->next;
    }
    return NULL;
}

void hashtable_delete(hashtable h, const void* key) {
    void *result = hashtable_pop(h, key);
    if (result == NULL) {
        return;
    }
    h->free_value(result);
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
                return SIZE_MAX;
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
                h->free_key(ptr->key);
                h->free_value(ptr->element);
                free(ptr);
                ptr = next;
            }
        }
        free(h->entries);
    }
    free(h);
}
