#include <stdio.h>
#include <gs_psm.h>
#include <graph.h>
#include <kernel.h>
#include <tamtypes.h>
#include <draw.h>
#include <dma.h>
#include <dma_tags.h>
#include <packet.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <math3d.h>
#include <model.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

#define DRAWBUF_SIZE 20000

#define MESH_FILE "host:monkey.bin"

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

	// set GS mode
	graph_set_mode(GRAPH_MODE_NONINTERLACED, GRAPH_MODE_VGA_1024_60, GRAPH_MODE_FRAME, GRAPH_DISABLE);
	graph_set_screen(0, 0, framebuf->width, framebuf->height);
	graph_set_bgcolor(0, 0, 0);
	graph_set_framebuffer_filtered(framebuf->address, framebuf->width, framebuf->psm, 0, 0);
	graph_enable_output();

	// setup draw environment

	packet_t *packet = packet_init(16, PACKET_NORMAL);

	qword_t *q = packet->data;

	q = draw_setup_environment(q, 0, framebuf, zbuf);

	// set drawing offset
	q = draw_primitive_xyoffset(q, 0, 2048-(SCREEN_WIDTH/2), 2048-(SCREEN_HEIGHT/2));

	q = draw_finish(q);

	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
	dma_wait_fast();

	packet_free(packet);

	return 0;
}

VECTOR tri_pos = {0.0f, 0.0f, 0.0f, 1.0f};
VECTOR tri_rot = {0.0f, 0.0f, 0.0f, 1.0f};
VECTOR tri_scale = {10.0f, 10.0f, 10.0f, 1.0f};

VECTOR cam_pos = {0.0f, 0.0f, 100.0f, 1.0f};
VECTOR cam_rot = {0.0f, 0.0f, 0.0f, 1.0f};


int draw(framebuffer_t *framebuf, zbuffer_t *zbuf) {
	
	int ctx = 0;

	packet_t *packets[2];
	packet_t *current;
	qword_t *dmatag;

	mesh_t *mesh = load_model(MESH_FILE);
            
	packets[0] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);
	packets[1] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);

	MATRIX local_world, view_screen, world_view, local_screen;

	// calculation space
	vertex_f_t *temp_verts = memalign(128, sizeof(vertex_f_t) * mesh->vertex_count);

	xyz_t *screen_verts = memalign(128, sizeof(xyz_t) * mesh->vertex_count);
	color_t *colors = memalign(128, sizeof(color_t) * mesh->vertex_count);

	prim_t prim;

	prim.type = PRIM_TRIANGLE;
	prim.shading = PRIM_SHADE_GOURAUD;
	prim.mapping = DRAW_DISABLE;
	prim.fogging = DRAW_DISABLE;
	prim.blending = DRAW_DISABLE;
	prim.antialiasing = DRAW_DISABLE;
	prim.mapping_type = PRIM_MAP_ST;
	prim.colorfix = PRIM_UNFIXED;

	color_t color = {
		.r = 255,
		.g = 255,
		.b = 255,
		.a = 255,
		.q = 1.0f
	};


	// set up matrices

	create_view_screen(view_screen, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

	dma_wait_fast();

	int i;
	while (1)
	{
		current = packets[ctx];
		dmatag = current->data;

		tri_rot[1] += 0.01f;

		create_local_world(local_world, tri_pos, tri_rot);
		matrix_scale(local_world, local_world, tri_scale);

		create_world_view(world_view, cam_pos, cam_rot);

		create_local_screen(local_screen, local_world, world_view, view_screen);

		calculate_vertices((VECTOR*)temp_verts, mesh->vertex_count, (VECTOR*)mesh->vertices, local_screen);

		draw_convert_xyz(screen_verts, 2048, 2048, 32, mesh->vertex_count, temp_verts);

		draw_convert_rgbq(colors, mesh->vertex_count, temp_verts, mesh->colors, 255);


		qword_t *q = dmatag;
		q++; // skip the header

		q = draw_disable_tests(q, 0, zbuf);
		q = draw_clear(q, 0, 2048.0f - (SCREEN_WIDTH/2), 2048.0f - (SCREEN_HEIGHT/2), framebuf->width, framebuf->height, 0, 0, 0);
		q = draw_enable_tests(q, 0, zbuf);

		q = draw_prim_start(q, 0, &prim, &color);

		for (i = 0; i < mesh->index_count; i++)
		{
			q->dw[0] = colors[mesh->indices[i]].rgbaq;
			q->dw[1] = screen_verts[mesh->indices[i]].xyz;
			q++;
		}

		q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);

		q = draw_finish(q);

		DMATAG_END(dmatag,(q-current->data)-1,0,0,0);

		dma_wait_fast();
		dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);

		ctx ^= 1;

		draw_wait_finish();
		graph_wait_vsync();
	}

	free(temp_verts);
	free(screen_verts);
	free(colors);

	packet_free(packets[0]);
	packet_free(packets[1]);

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

	printf("Drawing...\n");

	draw(&framebuf, &zbuf);

	return 0;
}