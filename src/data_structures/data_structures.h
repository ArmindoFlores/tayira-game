#ifndef _H_DATA_STRUCTURES_H_
#define _H_DATA_STRUCTURES_H_

#include <stddef.h>
#include <stdint.h>

/**
 * This enum represents the result of an iteration on a data structure.
 * Returning `ITERATION_BREAK` at results in an early stop in the iteration. 
 */
typedef enum iteration_result {
    ITERATION_CONTINUE,
    ITERATION_BREAK
} iteration_result;

typedef void (*free_function)(void*);
typedef void* (*copy_function)(const void*, size_t element_size);
typedef uint64_t (*hash_function)(const void*);
typedef int (*compare_function)(const void*, const void*);

typedef int (*predicate_function) (void *element, void *args);

#endif
