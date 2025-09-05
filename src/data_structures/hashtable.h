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
    void *key;
    void *value;
} hashtable_entry;

/**
 * This method creates a new hashtable. The returned object must be destroyed
 * using `hashtable_destroy(...)`.
 * 
 * @param key_size the size of the object pointed to by a key
 * @param free_key a function to free the resources associated with a key; if
 * `NULL`, then the key will be owned by the caller
 * @param copy_key a function that takes a key and returns a copy; if `NULL`,
 * it will simply copy the pointer
 * @param compare_keys a function that takes in two keys and returns 0 if they
 * are the same; if `NULL`, it will compare the raw pointers
 * @param hash_key a function that takes in a key and returns its hash; if `NULL`,
 * it will cast the key pointer to a `uint64_t`
 * @param value_size the size of the value pointed to by the value object
 * @param free_value a function to free the resources associated with a value; if 
 * `NULL`, then the value will be owned by the caller
 * @param copy_value a function that takes a value and returns a copy; if `NULL`,
 * it will simply copy the pointer
 * @param compare_values a function that takes in two keys and returns 0 if they are
 * the same; if `NULL`, it will compare the raw pointers
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 */
hashtable hashtable_create(
    size_t key_size,
    free_function free_key,
    copy_function copy_key,
    compare_function compare_keys,
    hash_function hash_key,
    size_t value_size,
    free_function free_value,
    copy_function copy_value
);

/**
 * This method creates a new hashtable with a string (`char *`) key 
 * owned by the hashtable (the key provided when calling `hashtable_set(...)`
 * is copied) and a value pointing to caller-owned memory.
 * 
 * When destroyed with `hashtable_destroy(...)`, the original keys passed
 * by the user when inserting into the hashtable will be intact, and the
 * internal ones will be automatically freed. The values associated with
 * the keys will also be intact.
 * 
 * It can be thought of the hashtable holding a copy of the key and a weak
 * reference to the value.
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 * 
 */
hashtable hashtable_create_copied_string_key_borrowed_pointer_value();

/**
 * This method creates a new hashtable with a string (`char *`) key 
 * owned by the hashtable (the key provided when calling `hashtable_set(...)`
 * is copied) and a value owned by the hashtable, but not copied.
 * 
 * When destroyed with `hashtable_destroy(...)`, the original keys passed
 * by the user when inserting into the hashtable will be intact, and the
 * internal ones will be automatically freed. The values associated with
 * the keys will be freed.
 * 
 * @param free_value a function to free the resources associated with a value;
 * this call will always fail if `free_value == NULL`
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 * 
 */
hashtable hashtable_create_copied_string_key_owned_pointer_value(
    free_function free_value
);

/**
 * This method creates a new hashtable with key of a non-pointer type that is
 * copied when calling `hashtable_set(...)` and a value pointing to caller-owned
 * memory.
 * 
 * When destroyed with `hashtable_destroy(...)`, the original keys passed
 * by the user when inserting into the hashtable will be intact, and the
 * internal ones will be automatically freed. The values associated with
 * the keys will also be intact.
 * 
 * Example:
 * ```
 * hashtable table = hashtable_create_trivial_key_borrowed_pointer_value(
 *     sizeof(int),
 *     compare_ints,
 *     hash_int
 * );
 * char *dynamic_string = calloc(5, sizeof(char));
 * strcpy(dynamic_string, "test");
 * hashtable_set(table, 123, "Hello World");
 * hashtable_set(table, 321, dynamic_string);
 * hashtable_destroy(table);
 * free(dynamic_string);
 * ```
 * 
 * @param key_size the size of the key object
 * @param compare_keys a function that takes in two keys and returns 0 if they
 * are the same; this call will always fail if `compare_keys == NULL`
 * @param hash_key a function that takes in a key and returns its hash; this
 * call will always fail if `compare_keys == NULL`
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 * 
 */
hashtable hashtable_create_trivial_key_borrowed_pointer_value(
    size_t key_size,
    compare_function compare_keys,
    hash_function hash_key
);

/**
 * This method creates a new hashtable with key of a non-pointer type that is
 * copied when calling `hashtable_set(...)` and a value owned by the hashtable,
 * but not copied.
 * 
 * When destroyed with `hashtable_destroy(...)`, the original keys passed
 * by the user when inserting into the hashtable will be intact, and the
 * internal ones will be automatically freed. The values associated with
 * the keys will also be intact, while the internal ones will be freed.
 * 
 * Example:
 * ```
 * hashtable table = hashtable_create_trivial_key_owned_pointer_value(
 *     sizeof(int),
 *     compare_ints,
 *     hash_int,
 *     free
 * );
 * char *dynamic_string = calloc(5, sizeof(char));
 * strcpy(dynamic_string, "test");
 * hashtable_set(table, 321, dynamic_string);
 * hashtable_destroy(table); // No need to free anything else
 * ```
 * 
 * @param key_size the size of the key object
 * @param compare_keys a function that takes in two keys and returns 0 if they
 * are the same; this call will always fail if `compare_keys == NULL`
 * @param hash_key a function that takes in a key and returns its hash; this
 * call will always fail if `compare_keys == NULL`
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 * 
 */
hashtable hashtable_create_trivial_key_owned_pointer_value(
    size_t key_size,
    compare_function compare_keys,
    hash_function hash_key,
    free_function free_value
);

/**
 * This method creates a new hashtable with key of a non-pointer type that is
 * copied when calling `hashtable_set(...)` and a value owned by the hashtable,
 * but not copied.
 * 
 * When destroyed with `hashtable_destroy(...)`, the original keys passed
 * by the user when inserting into the hashtable will be intact, and the
 * internal ones will be automatically freed. The values associated with
 * the keys will also be intact, while the internal ones will be freed.
 * 
 * Example:
 * ```
 * hashtable table = hashtable_create_trivial_key_trivial_value(
 *     sizeof(int),
 *     compare_ints,
 *     hash_int,
 *     sizeof(float)
 * );
 * hashtable_set(table, 321, 12.4f);
 * hashtable_destroy(table);
 * ```
 * 
 * @param key_size the size of the key object
 * @param compare_keys a function that takes in two keys and returns 0 if they
 * are the same; this call will always fail if `compare_keys == NULL`
 * @param hash_key a function that takes in a key and returns its hash; this
 * call will always fail if `compare_keys == NULL`
 * @param value_size the size of the value object
 * 
 * @return the hashtable pointer or NULL if this operation failed.
 * 
 */
hashtable hashtable_create_trivial_key_trivial_value(
    size_t key_size,
    compare_function compare_keys,
    hash_function hash_key,
    size_t value_size
);

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
int hashtable_set(hashtable h, const void* key, void* element);

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
void *hashtable_get(hashtable h, const void* key);

/**
 * This method checks whether a specified key is present in a hashtable.
 * 
 * @param h the hashtable
 * @param key the key
 * 
 * @return 1 if the key exists in the hashtable, 0 otherwise
 */
int hashtable_has(hashtable h, const void* key);

/**
 * This method removes a specified key and its value from a hashtable,
 * returning the latter.
 * *IMPORTANT:* This will *not* call the specified freeing function on
 * the object even if one exists.
 * 
 * @param h the hashtable
 * @param key the key
 * 
 * @return the value associated with the specified key
 */
void *hashtable_pop(hashtable h, const void* key);

/**
 * This method removes a specified key and its value from a hashtable,
 * freeing resources according to the specified freeing functions.
 * 
 * @param h the hashtable
 * @param key the key
 */
void hashtable_delete(hashtable h, const void* key);

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
 * for each hashtable created with `hashtable_create_copied_string_key_borrowed_pointer_value()`.
 * 
 * @param h the hashtable
 */
void hashtable_destroy(hashtable h);

uint64_t hashtable_hash_string(const void*);
uint64_t hashtable_hash_int(const void*);
int hashtable_compare_strings(const void *, const void*);
int hashtable_compare_ints(const void *, const void*);
void *hashtable_copy_trivial(const void *, size_t);
void *hashtable_copy_string(const void *, size_t);

#endif
