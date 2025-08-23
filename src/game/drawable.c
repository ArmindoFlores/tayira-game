#include "drawable.h"
#include "animation.h"
#include <stdlib.h>


struct drawable_s {
    union {
        texture texture;
        animation animation;
    } resource;
    drawable_type type;
};

drawable drawable_create(void* texture_or_animation, drawable_type type) {
    drawable d = (drawable) malloc(sizeof(struct drawable_s));
    if (d == NULL) {
        return NULL;
    }
    drawable_set(d, texture_or_animation, type);
    return d;
}

void drawable_set(drawable d, void *texture_or_animation, drawable_type type) {
    switch (type) {
        case DRAWABLE_ANIMATION:
            d->resource.animation = (animation) texture_or_animation;
            break;
        case DRAWABLE_TEXTURE:
            d->resource.texture = (texture) texture_or_animation;
            break;
        default:
            d->resource.animation = NULL;
            break;
    }
    d->type = type;
}

int drawable_get_width(drawable d) {
    switch (d->type) {
        case DRAWABLE_ANIMATION:
            return animation_get_width(d->resource.animation);
        case DRAWABLE_TEXTURE:
            return texture_get_width(d->resource.texture);
        default:
            return 0;
    }
}

int drawable_get_height(drawable d) {
    switch (d->type) {
        case DRAWABLE_ANIMATION:
            return animation_get_height(d->resource.animation);
        case DRAWABLE_TEXTURE:
            return texture_get_height(d->resource.texture);
        default:
            return 0;
    }
}

void drawable_render(drawable d, renderer_ctx ctx, int x, int y, double time, render_anchor anchor) {
    switch (d->type) {
        case DRAWABLE_ANIMATION:
            animation_render(d->resource.animation, ctx, x, y, time, anchor);
            break;
        case DRAWABLE_TEXTURE:
            int anchor_x = 0, anchor_y = 0;
            if (anchor & RENDER_ANCHOR_BOTTOM) {
                anchor_y += texture_get_height(d->resource.texture);
            }
            if (anchor & RENDER_ANCHOR_RIGHT) {
                anchor_x += texture_get_width(d->resource.texture);
            }
            if (anchor_y == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_LEFT)) {
                anchor_y += texture_get_height(d->resource.texture) / 2;
            }
            if (anchor_x == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_TOP)) {
                anchor_x += texture_get_width(d->resource.texture) / 2;
            }
            renderer_draw_texture(ctx, d->resource.texture, (float) (x - anchor_x), (float) (y - anchor_y));
            break;
        default:
            break;
    }
}

void drawable_destroy(drawable d) {
    if (d == NULL) return;
    free(d);
}
