#include "dialog.h"
#include "game/font.h"
#include "utils/utils.h"
#include <stdlib.h>
#include <string.h>

typedef struct dialog_textures {
    texture top, left, right, bottom;
    texture top_left, top_right, bottom_left, bottom_right;
    texture main;
} dialog_textures;

struct dialog_s {
    asset_manager_ctx asset_mgr;
    font font;
    dialog_textures textures;
    char *base_asset_id;
    char *texture_id_buffer;

    char *current_text;
    size_t current_text_length, current_char_index;

    int dialog_x, dialog_y, dialog_w, dialog_h;

    int has_next_page;
    double animation_delay, animation_start;

    dialog_status status;
};

static int load_dialog_textures(dialog d) {
    size_t asset_id_length = strlen(d->base_asset_id);
    d->texture_id_buffer = (char *) calloc(asset_id_length + 14, sizeof(char));
    if (d->texture_id_buffer == NULL) {
        return 1;
    }

    strcpy(d->texture_id_buffer, d->base_asset_id);
    strcat(d->texture_id_buffer, "/");

    strcpy(d->texture_id_buffer + asset_id_length + 1, "top");
    d->textures.top = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "left");
    d->textures.left = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "right");
    d->textures.right = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom");
    d->textures.bottom = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "top-left");
    d->textures.top_left = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "top-right");
    d->textures.top_right = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom-left");
    d->textures.bottom_left = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom-right");
    d->textures.bottom_right = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    strcpy(d->texture_id_buffer + asset_id_length + 1, "main");
    d->textures.main = asset_manager_texture_preload(d->asset_mgr, d->texture_id_buffer);

    if (
        d->textures.top == NULL ||
        d->textures.left == NULL ||
        d->textures.right == NULL ||
        d->textures.bottom == NULL ||
        d->textures.top_left == NULL ||
        d->textures.top_right == NULL ||
        d->textures.bottom_left == NULL ||
        d->textures.bottom_right == NULL ||
        d->textures.main == NULL
    ) {
        log_error("Failed to load dialog '{s}'", d->base_asset_id);
        return 1;
    }
    return 0;
}

static void unload_dialog_textures(dialog d) {
    if (d->texture_id_buffer == NULL) return;
    size_t asset_id_length = strlen(d->base_asset_id);

    if (d->textures.top != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "top");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.left != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "left");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.right != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "right");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.bottom != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.top_left != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "top-left");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.top_right != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "top-right");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.bottom_left != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom-left");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.bottom_right != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "bottom-right");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }

    if (d->textures.main != NULL) {
        strcpy(d->texture_id_buffer + asset_id_length + 1, "main");
        asset_manager_texture_unload(d->asset_mgr, d->texture_id_buffer);
    }
}

dialog dialog_create(asset_manager_ctx asset_mgr, const char *asset_id, const char *font_id, double font_spacing, double font_size, double animation_delay) {
    dialog d = (dialog) calloc(1, sizeof(struct dialog_s));
    if (d == NULL) {
        return NULL;
    }
    d->animation_delay = animation_delay;
    d->asset_mgr = asset_mgr;
    d->base_asset_id = utils_copy_string(asset_id);
    if (d->base_asset_id == NULL) {
        dialog_destroy(d);
        return NULL;
    }
    if (load_dialog_textures(d) != 0) {
        dialog_destroy(d);
        return NULL;
    }
    d->font = font_create(asset_mgr, font_id, font_spacing, font_size);
    if (d->font == NULL) {
        dialog_destroy(d);
        return NULL;
    }
    if (font_load(d->font) != 0) {
        dialog_destroy(d);
        return NULL;
    }
    return d;
}

void dialog_skip_animation(dialog d) {
    d->animation_start = -1;
}

int dialog_set_text(dialog d, const char* text, int has_next_page) {
    free(d->current_text);
    d->current_text = utils_copy_string(text);
    if (d->current_text == NULL) {
        return 1;
    }
    d->current_text_length = strlen(d->current_text);
    d->current_char_index = 0;
    d->has_next_page = has_next_page;
    d->status = DIALOG_ANIMATING;
    d->animation_start = 0;
    return 0;
}

const char *dialog_get_text(dialog d) {
    return d->current_text;
}

void dialog_set_position(dialog d, int x, int y) {
    d->dialog_x = x;
    d->dialog_y = y;
}

void dialog_set_dimensions(dialog d, int w, int h) {
    d->dialog_w = w;
    d->dialog_h = h;
}

static void render_dialog_box(dialog d, renderer_ctx ctx, float x, float y, float w, float h) {
    float left_offset = texture_get_width(d->textures.left);
    float top_offset = texture_get_height(d->textures.top);

    float main_width = w - (left_offset + texture_get_width(d->textures.right));
    float main_height = h - (top_offset + texture_get_height(d->textures.bottom));

    renderer_draw_texture(ctx, d->textures.top_left, x, y);
    renderer_draw_texture(ctx, d->textures.top_right, x + left_offset + main_width, y);
    renderer_draw_texture(ctx, d->textures.bottom_left, x, y + top_offset + main_height);
    renderer_draw_texture(ctx, d->textures.bottom_right, x + left_offset + main_width, y + top_offset + main_height);
    
    renderer_draw_texture_with_dimensions(ctx, d->textures.top, x + left_offset, y, main_width, top_offset);
    renderer_draw_texture_with_dimensions(ctx, d->textures.bottom, x + left_offset, y + top_offset + main_height, main_width, texture_get_height(d->textures.bottom));
    renderer_draw_texture_with_dimensions(ctx, d->textures.left, x, y + top_offset, left_offset, main_height);
    renderer_draw_texture_with_dimensions(ctx, d->textures.right, x + left_offset + main_width, y + top_offset, texture_get_width(d->textures.right), main_height);
    
    renderer_draw_texture_with_dimensions(
        ctx,
        d->textures.main,
        x + left_offset,
        y + top_offset,
        main_width,
        main_height
    );
}

int dialog_render(dialog d, renderer_ctx ctx, double t) {
    if (d->current_text == NULL) {
        d->status = DIALOG_ANIMATION_FINISHED;
        return 1;
    }
    if (d->animation_start == 0) {
        d->animation_start = t;
    }
    size_t i = d->animation_delay > 0 ? (t - d->animation_start) / d->animation_delay : d->current_text_length;
    d->current_char_index = i > d->current_text_length ? d->current_text_length : i;
    d->status = d->current_char_index == d->current_text_length ? DIALOG_ANIMATION_FINISHED : DIALOG_ANIMATING;

    render_dialog_box(d, ctx, d->dialog_x, d->dialog_y, d->dialog_w, d->dialog_h);
    renderer_increment_layer(ctx);

    int horizontal_margin = texture_get_width(d->textures.left);
    int vertical_margin = texture_get_height(d->textures.top);

    return font_render_n_constrained(
        d->font,
        ctx,
        d->current_char_index,
        d->current_text,
        d->dialog_x + horizontal_margin,
        d->dialog_y + vertical_margin,
        d->dialog_w - horizontal_margin - 6,
        d->dialog_h - vertical_margin - 6,
        (color_rgb) { .r = 0.0, .g = 0.0, .b = 0.0 }
    );
}

dialog_status dialog_get_status(dialog d) {
    return d->status;
}

void dialog_destroy(dialog d) {
    if (d == NULL) return;
    unload_dialog_textures(d);
    if (d->font) {
        font_destroy(d->font);
    }
    free(d->base_asset_id);
    free(d->current_text);
    free(d->texture_id_buffer);
    free(d);
}
