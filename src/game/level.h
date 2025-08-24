#ifndef _H_LEVEL_H_
#define _H_LEVEL_H_

#include "asset_manager.h"
#include "map.h"
#include "renderer/renderer.h"

typedef struct level_s *level;

level level_create(asset_manager_ctx, const char *level_id);
int level_render(level, renderer_ctx);
map level_get_map(level);
int level_load(level);
void level_unload(level);
void level_destroy(level);

#endif
