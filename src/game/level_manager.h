#ifndef _H_LEVEL_H_
#define _H_LEVEL_H_

#include "asset_manager.h"
#include "entity_manager.h"
#include "map.h"
#include "renderer/renderer.h"

typedef struct level_s *level;
typedef struct level_manager_ctx_s *level_manager_ctx;

level_manager_ctx level_manager_init(asset_manager_ctx, entity_manager_ctx);
level level_manager_load_level(level_manager_ctx, const char *level_id);
void level_manager_cleanup(level_manager_ctx);

level level_create(asset_manager_ctx, entity_manager_ctx, const char *level_id, const char *map_id);
int level_render(level, renderer_ctx, double t);
map level_get_map(level);
entity level_get_player_entity(level);
int level_load(level);
void level_unload(level);
void level_destroy(level);

#endif
