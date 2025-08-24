#ifndef _H_CONFIG_H_
#define _H_CONFIG_H_

static const char ASSETS_PATH_PREFIX[] = "assets/";
static const char ANIM_CONFIG_FILE_EXT[] = ".anim-config.json";
static const char ASSET_CONFIG_FILE_EXT[] = ".asset-config.json";
static const char FONT_CONFIG_FILE_EXT[] = ".font-config.json";
static const char MAP_CONFIG_FILE_EXT[] = ".map-config.json";
static const char LEVEL_CONFIG_FILE_EXT[] = ".level-config.json";
static const char ENTITY_CONFIG_FILE_EXT[] = ".entity-config.json";

typedef enum direction_e {
    DIRECTION_NONE,
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT
} direction;

#endif
