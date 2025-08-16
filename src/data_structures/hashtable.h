#ifndef _H_HASHTABLE_H_
#define _H_HASHTABLE_H_

#include "data_structures.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hashtable_s* hashtable;
typedef struct {
    const char *key;
    void *value;
} hashtable_entry;

hashtable hashtable_create();
int hashtable_set(hashtable, const char* key, void* element);
void *hashtable_get(hashtable, const char* key);
int hashtable_has(hashtable, const char* key);
int hashtable_delete(hashtable, const char* key);
size_t hashtable_foreach(hashtable, iteration_result (*callback) (const hashtable_entry*));
size_t hashtable_foreach_args(hashtable, iteration_result (*callback) (const hashtable_entry*, void* args), void* args);
void hashtable_destroy(hashtable);

#endif
