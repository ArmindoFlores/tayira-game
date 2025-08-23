#ifndef _H_DRAWABLE_H_
#define _H_DRAWABLE_H_

#include "renderer/renderer.h"

typedef struct drawable_s *drawable;
typedef enum drawable_type {
    DRAWABLE_EMPTY,
    DRAWABLE_ANIMATION,
    DRAWABLE_TEXTURE
} drawable_type;

drawable drawable_create(void* texture_or_animation, drawable_type);
void drawable_set(drawable, void* texture_or_animation, drawable_type);
int drawable_get_width(drawable);
int drawable_get_height(drawable);
void drawable_render(drawable, renderer_ctx, int x, int y, double time, render_anchor);
void drawable_destroy(drawable);

#endif
