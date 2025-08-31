#ifndef _H_PATHFINDING_H_
#define _H_PATHFINDING_H_

#include "data_structures/linked_list.h"

typedef struct integer_position {
    int x, y;
} integer_position;

linked_list pathfinding_find_path(int* occupancy_grid, int width, int height, integer_position start, integer_position goal);

#endif
