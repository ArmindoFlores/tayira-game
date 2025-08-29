#ifndef _H_HASHTABLE_H_
#define _H_HASHTABLE_H_

#include "data_structures.h"
#include <stddef.h>
#include <stdint.h>

/**
 * This type represents an opaque pointer to a hashtable.
 */
typedef struct hashtable_s* hashtable;

/**
 * This type represents a key-value pair from a hashtable.
 */
typedef struct {
    const char *key;
    void *value;
} hashtable_entry;

/**
 * This method creates a new hashtable. The returned object must be destroyed
 * using `hashtable_destroy(...)`.
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 */
hashtable hashtable_create();

/**
 * This method sets assigns the value for a specified key in the
 * hashtable. The value pointed to by `element` is not _owned_ by
 * the hashtable, and must still be freed by the user (if heap 
 * allocated).
 * 
 * @param h the hashtable
 * @param key the key to assign the value to
 * @param element the value to be assigned to the key
 * 
 * @return 0 if the operation is successful, 1 otherwise
 */
int hashtable_set(hashtable h, const char* key, void* element);

/**
 * This method gets the the value associated with the specified key
 * from a hashtable.
 * 
 * @param h the hashtable
 * @param key the key
 * 
 * @return the value associated with the key, or NULL if the key is not
 * in the hashtable
 */
void *hashtable_get(hashtable h, const char* key);

/**
 * This method checks whether a specified key is present in a hashtable.
 * 
 * @param h the hashtable
 * @param key the key
 * 
 * @return 1 if the key exists in the hashtable, 0 otherwise
 */
int hashtable_has(hashtable h, const char* key);

/**
 * This method removes a specified key and its value from a hashtable.
 * 
 * @param h the hashtable
 * @param key the key
 */
void *hashtable_delete(hashtable h, const char* key);

/**
 * This function iterates through every key-value pair in the hashtable and calls
 * the callback function each time with the pair as its arguments. If the function 
 * returns `ITERATION_CONTINUE`, iteration continues, otherwise, if it returns 
 * `ITERATION_BREAK`, the loop will exit.
 * 
 * @param h the hashtable
 * @param callback the callback function
 * 
 * @return the number of visited key-value pairs
 */
size_t hashtable_foreach(hashtable h, iteration_result (*callback) (const hashtable_entry*));

/**
 * This function iterates through every key-value pair in the hashtable and calls
 * the callback function each time with the pair as its arguments, and with an 
 * additional user-supplied argument. If the function returns `ITERATION_CONTINUE`, 
 * iteration continues, otherwise, if it returns `ITERATION_BREAK`, the loop will exit.
 * 
 * @param h the hashtable
 * @param callback the callback function
 * @param args the arguments to pass to the callback function in each iteration
 * 
 * @return the number of visited key-value pairs
 */
size_t hashtable_foreach_args(hashtable h, iteration_result (*callback) (const hashtable_entry*, void* args), void* args);

/**
 * This function frees the resources taken up by a hashtable. It must be called
 * for each hashtable created with `hashtable_create()`.
 * 
 * @param h the hashtable
 */
void hashtable_destroy(hashtable h);

#endif
