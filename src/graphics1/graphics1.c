#include <stdio.h>
#include <gs_psm.h>
#include <graph.h>
#include <kernel.h>
#include <tamtypes.h>
#include <draw.h>
#include <dma.h>
#include <packet.h>
#include <unistd.h>
#include <string.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

#define DRAWBUF_SIZE 100

int init_gs(framebuffer_t *framebuf, zbuffer_t *zbuf) {
	
	// define a 32-bit framebuffer
	framebuf->width = SCREEN_WIDTH;
	framebuf->height = SCREEN_HEIGHT;
	framebuf->mask = 0;
	framebuf->psm = GS_PSM_32;
	framebuf->address = graph_vram_allocate(framebuf->width, framebuf->height, framebuf->psm, GRAPH_ALIGN_PAGE);
	
	// define a 32-bit zbuffer
	zbuf->enable = DRAW_ENABLE;
	zbuf->mask = 0;
	zbuf->address = graph_vram_allocate(framebuf->width, framebuf->height, GS_PSM_32, GRAPH_ALIGN_PAGE);
	zbuf->method = ZTEST_METHOD_GREATER_EQUAL;
	zbuf->zsm = GS_ZBUF_32;

	graph_set_mode(GRAPH_MODE_NONINTERLACED, GRAPH_MODE_VGA_1024_60, GRAPH_MODE_FRAME, GRAPH_DISABLE);
	graph_set_screen(0, 0, framebuf->width, framebuf->height);
	graph_set_bgcolor(0, 0, 0);
	graph_set_framebuffer_filtered(framebuf->address, framebuf->width, framebuf->psm, 0, 0);
	graph_enable_output();

	packet_t *packet = packet_init(16, PACKET_NORMAL);

	qword_t *q = packet->data;

	q = draw_setup_environment(q, 0, framebuf, zbuf);

	q = draw_primitive_xyoffset(q, 0, 2048-(SCREEN_WIDTH/2), 2048-(SCREEN_HEIGHT/2));

	q = draw_finish(q);

	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
	dma_wait_fast();

	packet_free(packet);

	return 0;
}

int main(void) {

	printf("Initializing DMAC GIF channel...\n");

	dma_channel_initialize(DMA_CHANNEL_GIF,NULL,0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	printf("Initializing GS...\n");

	framebuffer_t framebuf;
	zbuffer_t zbuf;

	init_gs(&framebuf, &zbuf);

	graph_wait_vsync();

	packet_t *packet = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);

	while (1)
	{
		dma_wait_fast();

		qword_t *q = packet->data;

		memset(q, 0, DRAWBUF_SIZE * sizeof(qword_t));

		q = draw_disable_tests(q, 0, &zbuf);
		q = draw_clear(q, 0, 2048.0f - (SCREEN_WIDTH/2), 2048.0f - (SCREEN_HEIGHT/2), framebuf.width, framebuf.height, 0x20, 0x20, 0x20);
		q = draw_enable_tests(q, 0, &zbuf);

		q = draw_finish(q);

		dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

		draw_wait_finish();
		graph_wait_vsync();
	}
	

	return 0;
}