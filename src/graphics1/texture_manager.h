#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <draw_buffers.h>
#include "model.h"
#include <packet.h>
#include <dma.h>
#include <graph_vram.h>
#include <draw_sampling.h>
#include "clut.h"

extern clutbuffer_t no_clut;

clutbuffer_t* load_clut_in_vram(clut_t *clut);
texbuffer_t* load_texture_in_vram(texture_t *texture);
int unload_texture_from_vram(texbuffer_t *texbuf);
void setup_texture(texbuffer_t *texbuf, clutbuffer_t *clutbuf);

#endif