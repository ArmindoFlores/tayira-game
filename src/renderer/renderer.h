#ifndef _H_RESOURCES_H_
#define _H_RESOURCES_H_

#include "logger/logger.h"

typedef struct renderer_ctx_s *renderer_ctx;

renderer_ctx renderer_init(int width, int height, const char *title);
void renderer_fill(renderer_ctx, float r, float g, float b);
void renderer_run(
    renderer_ctx, 
    int (*update) (renderer_ctx), 
    int (*update) (renderer_ctx), 
    int (*update) (renderer_ctx),
    int (*update) (renderer_ctx)
);
void renderer_cleanup(renderer_ctx);

#endif
