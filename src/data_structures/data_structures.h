#ifndef _H_DATA_STRUCTURES_H_
#define _H_DATA_STRUCTURES_H_

/**
 * This enum represents the result of an iteration on a data structure.
 * Returning `ITERATION_BREAK` at results in an early stop in the iteration. 
 */
typedef enum iteration_result {
    ITERATION_CONTINUE,
    ITERATION_BREAK
} iteration_result;

typedef int (*predicate_function) (void *element, void *args);

#endif
