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

    unsigned char *pixels;
    unsigned char flags;

    GLuint id;
};

struct texture_s {
    GLuint asset_id;
    int width, height, offset_x, offset_y;
    float vertices[16];
};

static int asset_printer_fn(FILE *stream, const char *, va_list args) {
    asset a = va_arg(args, asset);
    if (a == NULL) {
        fprintf(stream, "(asset) {NULL}");
        return 0;
    }
    fprintf(
        stream,
        "(asset) { .width = %d, .height = %d, .channels = %d, .pixels = %p, .flags = %d, .id = %u }",
        a->width,
        a->height,
        a->channels,
        a->pixels,
        (int) a->flags,
        a->id
    );
    return 0;
}

static int texture_printer_fn(FILE *stream, const char *, va_list args) {
    texture t = va_arg(args, texture);
    if (t == NULL) {
        fprintf(stream, "(texture) {NULL}");
        return 0;
    }
    fprintf(
        stream,
        "(texture) { .width = %d, .height = %d, .offset_x = %d, .offset_y = %d, .asset_id = %u }",
        t->width,        
        t->height,
        t->offset_x,
        t->offset_y,
        t->asset_id
    );
    return 0;
}

asset asset_load(const char* filename, int tiled) {
    asset a = (asset) malloc(sizeof(struct asset_s));
    if (a == NULL) {
        return NULL;
    }
    a->id = 0;
    a->flags = 0;
    if (tiled) {
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

unsigned int asset_get_id(asset a) {
    return a->id;
}

void asset_unload(asset a) {
    if (a == NULL) return;
    stbi_image_free(a->pixels);
    asset_gpu_cleanup(a);
    free(a);
}

void asset_register_log_printer() {
    log_register_printer("asset", asset_printer_fn);
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

    GLuint tex_id = 0;
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

int asset_get_height(asset a) {
    return a->height;
}

int asset_get_width(asset a) {
    return a->width;
}

void asset_gpu_cleanup(asset a) {
    if (!asset_is_gpu_loaded(a)) return;
    glDeleteTextures(1, &a->id);
    a->id = 0;
    a->flags &= ~ASSET_GPU_LOADED;
}

texture texture_from_asset(asset a, int width, int height, int offset_x, int offset_y) {
    if (!asset_is_gpu_loaded(a)) {
        log_error("Tried to instanciate a texture from an unloaded asset");
        return NULL;
    }

    texture t = (texture) malloc(sizeof(struct texture_s));
    if (t == NULL) {
        return NULL;
    }

    t->asset_id = a->id;
    t->width = width;
    t->height = height;
    t->offset_x = offset_x;
    t->offset_y = offset_y;
    
    float sub_x = (float) offset_x;
    float sub_y = (float) offset_y;

    float u0 = sub_x / a->width;
    float v0 = sub_y / a->height;
    float u1 = (sub_x + width) / a->width;
    float v1 = (sub_y + height) / a->height;

    if (sub_x + width > a->width || sub_y + height > a->height) {
        log_error("{texture} indexing out of {asset} bounds", t, a);
        free(t);
        return NULL;
    }

    float vertices[] = {
        0.0f, 1.0f, u0, v0,
        1.0f, 1.0f, u1, v0,
        1.0f, 0.0f, u1, v1,
        0.0f, 0.0f, u0, v1
    };
    memcpy(t->vertices, vertices, sizeof(vertices));

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

int texture_get_offset_x(texture t) {
    return t->offset_x;
}

int texture_get_offset_y(texture t) {
    return t->offset_y;
}

void texture_get_vertices(texture t, float vertices[16]) {
    memcpy(vertices, t->vertices, sizeof(t->vertices));
}

void texture_destroy(texture t) {
    free(t);
}

void texture_register_log_printer() {
    log_register_printer("texture", texture_printer_fn);
}
