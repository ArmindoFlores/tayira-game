#ifndef _H_MAP_H_
#define _H_MAP_H_

#include "ai/pathfinding.h"
#include "asset_manager.h"
#include "renderer/renderer.h"

typedef struct map_s *map;

map map_create(asset_manager_ctx, const char *map_id);
int map_render(map, renderer_ctx, unsigned int entity_layer_offset);
int map_occupied_at(map, int x, int y);
linked_list map_find_path(map, integer_position from, integer_position to);
int map_load(map);
int map_unload(map);
void map_destroy(map);
void map_register_log_printer();

#endif