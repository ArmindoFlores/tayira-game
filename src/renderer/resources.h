#ifndef _H_RESOURCES_H_
#define _H_RESOURCES_H_

typedef struct texture_s* texture;

texture rsrc_texture_load(const char* filename);
void rsrc_texture_unload(texture);


#endif