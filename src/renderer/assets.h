#ifndef _H_ASSETS_H_
#define _H_ASSETS_H_

enum asset_flags_e {
    ASSET_PERMANENT     = 0b00000001,
    ASSET_TILED         = 0b00000010,
    ASSET_GPU_LOADED    = 0b00000100
};

typedef struct asset_s* asset;
typedef struct texture_s* texture;

asset asset_load(const char* filename, int tile_size);
int asset_is_gpu_loaded(asset);
int asset_is_permanent(asset);
int asset_is_tiled(asset);
int asset_to_gpu(asset);
unsigned int asset_get_id(asset);
void asset_gpu_cleanup(asset);
void asset_unload(asset);

texture texture_from_asset(asset, int index);
unsigned int texture_get_id(texture);
int texture_get_height(texture);
int texture_get_width(texture);
void texture_get_vertices(texture, float vertices[16]);
void texture_destroy(texture);

#endif
