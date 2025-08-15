#ifndef _H_ASSET_MANAGER_H_
#define _H_ASSET_MANAGER_H_

#include "renderer/assets.h"

typedef struct asset_manager_ctx_s* asset_manager_ctx;

asset_manager_ctx asset_manager_init();
asset asset_manager_asset_gpu_preload(asset_manager_ctx, const char* asset_id);
asset asset_manager_asset_preload(asset_manager_ctx, const char* asset_id);
int asset_manager_asset_unload(asset_manager_ctx, const char* asset_id);
texture asset_manager_texture_preload(asset_manager_ctx, const char* texture_id);
texture asset_manager_get_texture(asset_manager_ctx, const char* texture_id);
void asset_manager_texture_unload(asset_manager_ctx, const char* texture_id);
void asset_manager_cleanup(asset_manager_ctx);

#endif
