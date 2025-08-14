#define STB_IMAGE_IMPLEMENTATION

#include "assets.h"
#include "stb_image.h"
#include "logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

struct asset_s {
    int width;
    int height;
    int channels;
    
    int tile_size;

    unsigned char *pixels;
    unsigned char flags;

    GLuint id;
};

struct texture_s {
    GLuint asset_id;
    int width, height;
    float vertices[16];
};

asset asset_load(const char* filename, int tile_size) {
    asset a = (asset) malloc(sizeof(struct asset_s));
    if (a == NULL) {
        return NULL;
    }
    a->flags = 0;
    a->tile_size = tile_size;
    if (tile_size > 0) {
        a->flags |= ASSET_TILED;
    }
    a->pixels = stbi_load(filename, &a->width, &a->height, &a->channels, 4);
    if (a->pixels == NULL) {
        free(a);
        return NULL;
    }
    return a;
}

int asset_is_gpu_loaded(asset a) {
    return (a->flags & ASSET_GPU_LOADED) > 0;
}

int asset_is_permanent(asset a) {
    return (a->flags & ASSET_PERMANENT) > 0;
}

int asset_is_tiled(asset a) {
    return (a->flags & ASSET_TILED) > 0;
}

void asset_unload(asset a) {
    if (a == NULL) return;
    stbi_image_free(a->pixels);
    asset_gpu_cleanup(a);
    free(a);
}

static GLenum channels_to_format(int channels) {
    switch (channels) {
        case 1:  return GL_RED;
        case 2:  return GL_RG;
        case 3:  return GL_RGB;
        case 4:  return GL_RGBA;
        default: return GL_RGBA8;
    }
}

int asset_to_gpu(asset a) {
    if (!a || !a->pixels) return 1;

    if (asset_is_gpu_loaded(a)) {
        log_warning("Loading previously loaded asset to GPU");
    }

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    // Default parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = channels_to_format(a->channels);

    // Upload pixels
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        a->width,
        a->height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        a->pixels
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    a->id = tex_id;
    a->flags |= ASSET_GPU_LOADED;
    return 0;
}

void asset_gpu_cleanup(asset a) {
    if (!asset_is_gpu_loaded(a)) return;
    glDeleteTextures(1, &a->id);
    a->id = 0;
    a->flags &= ~ASSET_GPU_LOADED;
}

texture texture_from_asset(asset a, int index) {
    if (!asset_is_gpu_loaded(a)) {
        log_error("Tried to instanciate a texture from an unloaded asset");
        return NULL;
    }

    texture t = (texture) malloc(sizeof(struct texture_s));
    if (t == NULL) {
        return NULL;
    }

    t->asset_id = a->id;
    
    if (asset_is_tiled(a)) {
        int cols = a->width / a->tile_size;
        int rows = a->height / a->tile_size;

        int col = index % cols;
        int row = index / cols;

        if (row > rows) {
            free(t);
            log_error("Tried to load a texture from an asset at (%d, %d), but asset has dimensions (%d, %d)", col, row, cols, rows);
            return NULL;
        }

        float sub_x = (float) col * a->tile_size;
        float sub_y = (float) row * a->tile_size;

        float u0 = sub_x / a->width;
        float v0 = sub_y / a->height;
        float u1 = (sub_x + a->tile_size) / a->width;
        float v1 = (sub_y + a->tile_size) / a->height;

        float vertices[] = {
            0.0f, 1.0f, u0, v0,
            1.0f, 1.0f, u1, v0,
            1.0f, 0.0f, u1, v1,
            0.0f, 0.0f, u0, v1
        };
        memcpy(t->vertices, vertices, sizeof(vertices));
        t->width = a->tile_size;
        t->height = a->tile_size;
    }
    else {
        float vertices[] = {
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        };
        memcpy(t->vertices, vertices, sizeof(vertices));
        t->width = a->width;
        t->height = a->height;
    }

    return t;
}

unsigned int texture_get_id(texture t) {
    return t->asset_id;
}

int texture_get_height(texture t) {
    return t->height;
}

int texture_get_width(texture t) {
    return t->width;
}

void texture_get_vertices(texture t, float vertices[16]) {
    memcpy(vertices, t->vertices, sizeof(t->vertices));
}

void texture_destroy(texture t) {
    free(t);
}
