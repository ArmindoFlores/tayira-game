#ifndef _H_ANIMATION_H_
#define _H_ANIMATION_H_

#include "asset_manager.h"
#include "renderer/renderer.h"

typedef struct animation_s *animation;

animation animation_create(asset_manager_ctx, const char *asset_id, const char *variant);
int animation_render(animation, renderer_ctx, int x, int y, double time, render_anchor);
int animation_render_bounds(animation, renderer_ctx, int x, int y, render_anchor);
int animation_load(animation);
int animation_unload(animation);
void animation_destroy(animation);
void animation_register_log_printer();

#endif
