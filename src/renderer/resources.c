#define STB_IMAGE_IMPLEMENTATION

#include "resources.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

struct texture_s {
    int width, height, channels;
    unsigned char *data;
};

texture rsrc_texture_load(const char* filename) {
    texture t = (texture) malloc(sizeof(struct texture_s));
    if (t == NULL) {
        return NULL;
    }
    t->data = stbi_load(filename, &t->width, &t->height, &t->channels, 4);
    if (t->data == NULL) {
        free(t);
        return NULL;
    }
    return t;
}

void rsrc_texture_unload(texture t) {
    if (t == NULL) return;
    stbi_image_free(t->data);
    free(t);
}
