#include "font.h"
#include <stdlib.h>
#include <string.h>

struct font_s {
    asset_manager_ctx asset_mgr;
    char *base_asset_id;
    float spacing;
    float size;
    
    size_t asset_id_length;
    char *texture_id_buffer;
};

font font_create(asset_manager_ctx ctx, const char *asset_id, float spacing, float size){
    font f = (font) malloc(sizeof(struct font_s));
    if (f == NULL) {
        return NULL;
    }

    f->spacing = spacing;
    f->size = size;
    f->asset_id_length = strlen(asset_id);
    f->base_asset_id = calloc(f->asset_id_length + 1, sizeof(char));
    if (f->base_asset_id == NULL) {
        free(f);
        return NULL;
    }

    f->texture_id_buffer = calloc(f->asset_id_length + 3, sizeof(char));
    if (f->texture_id_buffer == NULL) {
        free(f->base_asset_id);
        free(f);
        return NULL;
    }

    strcpy(f->base_asset_id, asset_id);
    strcpy(f->texture_id_buffer, asset_id);
    f->texture_id_buffer[f->asset_id_length] = '/';
    f->asset_mgr = ctx;
    return f;
}

int font_render(font f, renderer_ctx renderer, const char* string, int x, int y) {
    float cur_x = (float) x;
    float cur_y = (float) y;
    size_t string_length = strlen(string);
    for (size_t i = 0; i < string_length; i++) {
        char c = string[i];
        
        // If the glyph is a space, just increase cur_x
        if (c == ' ') {
            cur_x += f->size + f->spacing;
            continue;
        }

        // If the glyph is a newline, increase cur_y instead
        if (c == '\n') {
            cur_x = (float) x;
            cur_y += f->size + f->spacing;
            continue;
        }

        // Try to get texture for this glyph
        f->texture_id_buffer[f->asset_id_length+1] = c;
        texture t = asset_manager_get_texture(f->asset_mgr, f->texture_id_buffer);
        texture_info *t_info = asset_manager_get_texture_info(f->asset_mgr, f->texture_id_buffer);
        if (t == NULL || t_info == NULL) {
            // TODO: render a fallback glyph, and only if that one isn't found, return early
            log_warning("Failed to render glyph '{c}'", c);
            return 1;
        }
        
        // Adjust y_pos based on supposed height
        float y_pos = cur_y + (f->size - t_info->height);

        renderer_draw_texture(renderer, t, cur_x, y_pos);
        cur_x += f->spacing + t_info->width;
    }
    return 0;
}

int font_load(font f) {
    return asset_manager_asset_and_textures_preload(f->asset_mgr, f->base_asset_id);
}

int font_unload(font f) {
    return asset_manager_asset_unload(f->asset_mgr, f->base_asset_id);
}

void font_destroy(font f) {
    if (f == NULL) return;
    font_unload(f);  // FIXME: may fail
    free(f->texture_id_buffer);
    free(f->base_asset_id);
    free(f);
}
