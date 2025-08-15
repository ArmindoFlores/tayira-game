#ifndef _H_HASHTABLE_H_
#define _H_HASHTABLE_H_

#include <stddef.h>
#include <stdint.h>

typedef struct hashtable_s* hashtable;
typedef struct hashtable_iterator_s* hashtable_iterator;
typedef struct {
    char *key;
    void *value;
} hashtable_entry;

hashtable hashtable_create();
int hashtable_set(hashtable, const char* key, void* element);
void *hashtable_get(hashtable, const char* key);
int hashtable_has(hashtable, const char* key);
int hashtable_delete(hashtable, const char* key);
hashtable_iterator hashtable_begin_iter(hashtable);
hashtable_entry hashtable_next_iter(hashtable_iterator);
void hashtable_end_iter(hashtable_iterator);
void hashtable_destroy(hashtable);

#endif
