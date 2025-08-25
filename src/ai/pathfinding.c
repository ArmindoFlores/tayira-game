#include "pathfinding.h"
#include "data_structures/hashtable.h"
#include "data_structures/heap.h"
#include "utils/utils.h"
#include "logger/logger.h"
#include <stdlib.h>

#define PREPARE_KEY(key) do { snprintf(hashtable_key_buffer, hashtable_key_buffer_size, "%d", key); } while(0)

int pathfinding_find_path(int** occupancy_grid, int width, int height, int start, int goal) {
    //! This is pretty inneficient since we're using our custom string-key-based hashtable.
    //! Thus, even if we're avoiding allocating memory in each iteration in this function,
    //! the hashtable itself will create a copy of the key.
    //! If this proves to be a bottleneck, an implementation of `hashtable` with integer
    //! keys might be preferrable.

    int return_value = 0;
    size_t max_index = width * height;
    size_t hashtable_key_buffer_size = utils_digit_length(max_index) + 1;
    char *hashtable_key_buffer = (char*) calloc(hashtable_key_buffer_size, sizeof(char));
    if (hashtable_key_buffer == NULL) {
        log_error("Failed to allocate memory during pathfinding");
        return 1;
    }

    heap open_set = NULL;
    hashtable came_from = NULL, g_score = NULL, f_score = NULL;
    open_set = heap_create(width * height / 2 + 1);
    came_from = hashtable_create();
    g_score = hashtable_create();
    f_score = hashtable_create();   
    
    if (open_set == NULL || came_from == NULL || g_score == NULL || f_score == NULL) {
        return_value = 1;
        goto cleanup;
    }

    PREPARE_KEY(start);
    // if (hashtable_set(g_score, hashtable_key_buffer, ))

cleanup:
    free(hashtable_key_buffer);
    heap_destroy(open_set);
    hashtable_destroy(came_from);
    hashtable_destroy(g_score);
    hashtable_destroy(f_score);
    return return_value;
}
