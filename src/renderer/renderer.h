#ifndef _H_RESOURCES_H_
#define _H_RESOURCES_H_

#include "logger/logger.h"
#include "assets.h"

typedef struct color_rgb {
    float r, g, b;
} color_rgb;

typedef struct renderer_statistics {
    size_t draw_calls;
    size_t drawn_instances;
} renderer_statistics;

typedef enum render_anchor {
    RENDER_ANCHOR_CENTER = 0b00000001,
    RENDER_ANCHOR_LEFT   = 0b00000010,
    RENDER_ANCHOR_RIGHT  = 0b00000100,
    RENDER_ANCHOR_TOP    = 0b00001000,
    RENDER_ANCHOR_BOTTOM = 0b00010000
} render_anchor;

typedef struct renderer_ctx_s *renderer_ctx;
typedef int (*key_callback) (renderer_ctx, int key, int scancode, int action, int mods);
typedef int (*mouse_button_callback) (renderer_ctx, int button, int action, int mods);
typedef int (*mouse_move_callback) (renderer_ctx, double x, double y);
typedef int (*update_callback) (renderer_ctx, double dt, double t);

renderer_ctx renderer_init(int width, int height, const char *title);
void renderer_register_user_context(renderer_ctx, void *user_context);
void *renderer_get_user_context(renderer_ctx);
unsigned int renderer_set_layer(renderer_ctx, unsigned int layer);
unsigned int renderer_get_layer(renderer_ctx);
unsigned int renderer_increment_layer(renderer_ctx);
unsigned int renderer_decrement_layer(renderer_ctx);
void renderer_fill(renderer_ctx, color_rgb);
void renderer_run(
    renderer_ctx, 
    update_callback update_cb, 
    key_callback key_cb,
    mouse_button_callback mouse_button_cb,
    mouse_move_callback mouse_move_cb,
    mouse_move_callback scroll_cb
);
void renderer_draw_texture(renderer_ctx, texture, float x, float y);
void renderer_set_tint(renderer_ctx, color_rgb);
void renderer_clear_tint(renderer_ctx);
void renderer_toggle_fullscreen(renderer_ctx);
renderer_statistics renderer_get_stats(renderer_ctx);
void renderer_cleanup(renderer_ctx);

#endif
