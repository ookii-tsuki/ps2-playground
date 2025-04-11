#include "texture_manager.h"
#include <malloc.h>
#include <draw.h>

clutbuffer_t no_clut = {
    .address = 0,
    .start = 0,
    .storage_mode = CLUT_STORAGE_MODE1,
    .load_method = CLUT_NO_LOAD,
    .psm = 0
};

clutbuffer_t* load_clut_in_vram(clut_t *clut) {
    clutbuffer_t *clutbuf = malloc(sizeof(clutbuffer_t));
    clutbuf->start = 0;
    clutbuf->storage_mode = CLUT_STORAGE_MODE1;
    clutbuf->load_method = CLUT_LOAD;
    clutbuf->psm = GS_PSM_32;
    
    int clut_width = clut->psm == GS_PSM_4 ? 8 : 16;
    int clut_height = clut->psm == GS_PSM_4 ? 2 : 16;
    
    clutbuf->address = graph_vram_allocate(clut_width, clut_height, clutbuf->psm, GRAPH_ALIGN_BLOCK);
    
    packet_t *packet = packet_init(20, PACKET_NORMAL);
    qword_t *q = packet->data;
    
    q = draw_texture_transfer(q, clut->palette, clut_width, clut_height,
                            GS_PSM_32, clutbuf->address, 64);
    
    q = draw_texture_flush(q);
    
    dma_wait_fast();
    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();
    packet_free(packet);
    
    return clutbuf;
}

texbuffer_t* load_texture_in_vram(texture_t *texture) {
    texbuffer_t *texbuf = malloc(sizeof(texbuffer_t));
    texbuf->width = texture->width;
    texbuf->psm = texture->psm;
    texbuf->address = graph_vram_allocate(texture->width, texture->height, texbuf->psm, GRAPH_ALIGN_BLOCK);
    
    packet_t *packet = packet_init(20, PACKET_NORMAL);
    qword_t *q = packet->data;
    
    q = draw_texture_transfer(q, texture->texture_data, texture->width, texture->height, 
                            texbuf->psm, texbuf->address, texbuf->width);
    
    q = draw_texture_flush(q);
    
    dma_wait_fast();
    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();
    packet_free(packet);
    
    return texbuf;
}

int unload_texture_from_vram(texbuffer_t *texbuf) {
    if (texbuf) {
        graph_vram_free(texbuf->address);
        free(texbuf);
        return 0;
    }
    return -1;
}

void setup_texture(texbuffer_t *texbuf, clutbuffer_t *clutbuf) {
	
	packet_t *packet = packet_init(16, PACKET_NORMAL);

	qword_t *q = packet->data;

	lod_t lod;

	lod.calculation = LOD_USE_K;
	lod.max_level = 0;
	lod.mag_filter = LOD_MAG_NEAREST;
	lod.min_filter = LOD_MIN_NEAREST;
	lod.l = 0;
	lod.k = 0;

	texbuf->info.width = draw_log2(texbuf->width);
	texbuf->info.height = draw_log2(texbuf->width);
	texbuf->info.components = TEXTURE_COMPONENTS_RGB;
	texbuf->info.function = TEXTURE_FUNCTION_DECAL;


	q = draw_texture_sampling(q, 0, &lod);

	q = draw_texturebuffer(q, 0, texbuf, clutbuf);

    dma_wait_fast();

	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

	dma_wait_fast();

	packet_free(packet);
}