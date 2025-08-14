#ifndef _H_RESOURCES_H_
#define _H_RESOURCES_H_

#include "logger/logger.h"

typedef struct renderer_ctx_s *renderer_ctx;
typedef int (*key_callback) (renderer_ctx, int key, int scancode, int action, int mods);
typedef int (*mouse_button_callback) (renderer_ctx, int button, int action, int mods);
typedef int (*mouse_move_callback) (renderer_ctx, double x, double y);
typedef int (*update_callback) (renderer_ctx, double dt);

renderer_ctx renderer_init(int width, int height, const char *title);
void renderer_fill(renderer_ctx, float r, float g, float b);
void renderer_run(
    renderer_ctx, 
    update_callback update_cb, 
    key_callback key_cb,
    mouse_button_callback mouse_button_cb,
    mouse_move_callback mouse_move_cb,
    mouse_move_callback scroll_cb
);
void renderer_cleanup(renderer_ctx);

#endif
