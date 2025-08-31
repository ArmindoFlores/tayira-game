// TODO: This file is a mess.

#include "pathfinding.h"
#include "data_structures/hashtable.h"
#include "data_structures/heap.h"
#include "utils/utils.h"
#include "logger/logger.h"
#include <stdlib.h>

#define POS_VALUE(x, y) (&(integer_position){ .x=x, .y=y })
#define INT_VALUE(x) (&(int){ x })
#define ASSERT_OR_EXIT(expr) do { if (!(expr)) { return_value = NULL; goto cleanup; } } while (0)
#define SUCCESS_OR_EXIT(expr) do { if ((expr) != 0) { return_value = NULL; goto cleanup; } } while (0)

static int compare_positions(const void* _value1, const void* _value2) {
    integer_position *value1 = (integer_position*) _value1, *value2 = (integer_position*) _value2;
    return (value1->x == value2->x && value1->y == value2->y) ? 0 : 1;
}

static uint64_t hash_position(const void *_key) {
    integer_position *key = (integer_position*) _key;
    return hashtable_hash_int(&(int){key->x}) ^ hashtable_hash_int(&(int){key->y});
}

static int heuristic(integer_position *goal, integer_position *pos) {
    int x_diff = goal->x - pos->x;
    int y_diff = goal->y - pos->y;
    return x_diff * x_diff + y_diff * y_diff;
}

static integer_position *new_position(int x, int y) {
    integer_position *pos = (integer_position*) malloc(sizeof(integer_position));
    if (pos == NULL) {
        return NULL;
    }
    pos->x = x;
    pos->y = y;
    return pos;
}

static iteration_result destroy_integer_position(void *element) {
    free(element);
    return ITERATION_CONTINUE;
}

linked_list reconstruct_path(hashtable came_from, integer_position *current) {
    // TODO: We will probably only need to return the next position, not a 
    // TODO: full path.

    linked_list total_path = linked_list_create();
    if (total_path == NULL) {
        return NULL;
    }

    integer_position *pos = new_position(current->x, current->y);
    if (pos == NULL) {
        linked_list_destroy(total_path);
        return NULL;
    }

    if (linked_list_pushfront(total_path, pos) != 0) {
        free(pos);
        linked_list_destroy(total_path);
        return NULL;
    }

    integer_position *from = current;
    while (1) {
        from = hashtable_get(came_from, from);
        if (from == NULL) break;

        integer_position *from_copy = new_position(from->x, from->y);
        if (from_copy == NULL) {
            linked_list_foreach(total_path, destroy_integer_position);
            linked_list_destroy(total_path);
            return NULL;
        }
        if (linked_list_pushfront(total_path, from_copy) != 0) {
            free(from_copy);
            linked_list_foreach(total_path, destroy_integer_position);
            linked_list_destroy(total_path);
            return NULL;
        }
    }

    return total_path;
}

static int find_element(void *element, void *args) {
    return compare_positions(element, args) == 0;
}

static void destroy_heap(heap h) {
    if (h == NULL) return;
    while (1) {
        void *element = NULL;
        heap_pop(h, &element, NULL);
        if (element == NULL) break;
        free(element);
    }
    heap_destroy(h);
}

linked_list pathfinding_find_path(int* occupancy_grid, int width, int height, integer_position start, integer_position goal) {
    //! FIXME: Although our hashtable implementation works with both heap-allocated values and by copying objects
    //! on the stack, the heap doesn't (yet).

    linked_list return_value = NULL;

    heap open_set = NULL;
    hashtable came_from = NULL, g_score = NULL, f_score = NULL;
    integer_position *pos = NULL;
    open_set = heap_create(width * height / 2 + 1);
    came_from = hashtable_create_trivial_key_trivial_value(sizeof(integer_position), compare_positions, hash_position, sizeof(integer_position));
    g_score = hashtable_create_trivial_key_trivial_value(sizeof(integer_position), compare_positions, hash_position, sizeof(int));
    f_score = hashtable_create_trivial_key_trivial_value(sizeof(integer_position), compare_positions, hash_position, sizeof(int));
    
    ASSERT_OR_EXIT(open_set != NULL && came_from != NULL && g_score != NULL && f_score != NULL);

    int h_start = heuristic(&goal, &start);
    pos = new_position(start.x, start.y);
    ASSERT_OR_EXIT(pos != NULL);
    SUCCESS_OR_EXIT(heap_insert(open_set, pos, h_start));
    pos = NULL;

    SUCCESS_OR_EXIT(hashtable_set(g_score, &start, INT_VALUE(0)));
    SUCCESS_OR_EXIT(hashtable_set(f_score, &start, &h_start));

    while (!heap_is_empty(open_set)) {
        integer_position *current_ptr = NULL; 
        integer_position neighbour, current;

        if (heap_pop(open_set, (void **) &current_ptr, NULL) == 1) break;
        current.x = current_ptr->x;
        current.y = current_ptr->y;
        free(current_ptr);

        log_debug("current=({d}, {d})", current.x, current.y);

        if (compare_positions(&current, &goal) == 0) {
            return_value = reconstruct_path(came_from, &current);
            goto cleanup;
        }

        int tentative_g_score = 0;
        void *tmp = NULL;

        for (int i = 0; i < 9; i++) {            
            int x_offset = (i % 3) - 1;
            int y_offset = (i / 3) - 1;

            if (abs(x_offset + y_offset) != 1) continue;

            neighbour = (integer_position) { .x = current.x + x_offset, .y = current.y + y_offset };
            
            if (neighbour.x >= width || neighbour.x < 0 || neighbour.y >= height || neighbour.y < 0) {
                // Out of bounds
                continue;
            }

            if (occupancy_grid[neighbour.x + neighbour.y * width] == 1) {
                // This cell is occupied
                continue;
            }

            tmp = hashtable_get(g_score, &current);
            if (tmp == NULL) {
                log_error("Pathfinding error: missing g_score");
                break;
            }
            tentative_g_score = *(int*) tmp + 1;  // TODO: instead of 1, it should be d(current, neighbour)
            
            tmp = hashtable_get(g_score, &neighbour);
            if (tmp == NULL || tentative_g_score < *(int*) tmp) {
                // This path is better, so record it
                SUCCESS_OR_EXIT(hashtable_set(came_from, &neighbour, &current));
                SUCCESS_OR_EXIT(hashtable_set(g_score, &neighbour, &tentative_g_score));
                
                int neighbour_heuristic = heuristic(&goal, &neighbour);
                SUCCESS_OR_EXIT(hashtable_set(f_score, &neighbour, INT_VALUE(tentative_g_score + neighbour_heuristic)));
                if (!heap_any(open_set, find_element, &neighbour)) {
                    integer_position *pos = new_position(neighbour.x, neighbour.y);
                    ASSERT_OR_EXIT(pos != NULL);
                    SUCCESS_OR_EXIT(heap_insert(open_set, pos, neighbour_heuristic));
                    pos = NULL;
                }            
            }
        }
    }

cleanup:
    free(pos);
    destroy_heap(open_set);
    hashtable_destroy(came_from);
    hashtable_destroy(g_score);
    hashtable_destroy(f_score);
    return return_value;
}
