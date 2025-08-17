#ifndef _H_FONT_H_
#define _H_FONT_H_

#include "asset_manager.h"
#include "renderer/renderer.h"

typedef struct font_s *font;

font font_create(asset_manager_ctx, const char *asset_id, float spacing, float size);
int font_render(font, renderer_ctx, const char*, int x, int y);
int font_load(font);
int font_unload(font);
void font_destroy(font);

#endif
