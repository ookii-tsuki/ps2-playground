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
#include <math.h>
#include <model.h>
#include <clut.h>

#define PI 3.1415926

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

#define DRAWBUF_SIZE 20000

#define MESH_FILE "host:car.bin"
#define CLUT_FILE "host:car.clt"

int init_gs(framebuffer_t *framebuf, zbuffer_t *zbuf, texbuffer_t *texbuf) {
	
	// allocate two framebuffers for double buffering
	framebuf[0].width = framebuf[1].width = SCREEN_WIDTH;
    framebuf[0].height = framebuf[1].height = SCREEN_HEIGHT;
    framebuf[0].mask = framebuf[1].mask = 0;
    framebuf[0].psm = framebuf[1].psm = GS_PSM_24;
	
    framebuf[0].address = graph_vram_allocate(framebuf[0].width, framebuf[0].height, framebuf[0].psm, GRAPH_ALIGN_PAGE);

    framebuf[1].address = graph_vram_allocate(framebuf[1].width, framebuf[1].height, framebuf[1].psm, GRAPH_ALIGN_PAGE);
	
	// define and allocate zbuffer
	zbuf->enable = DRAW_ENABLE;
	zbuf->mask = 0;
	zbuf->method = ZTEST_METHOD_GREATER_EQUAL;
	zbuf->zsm = GS_ZBUF_16;
	zbuf->address = graph_vram_allocate(framebuf->width, framebuf->height, zbuf->zsm, GRAPH_ALIGN_PAGE);

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


void load_texture(texbuffer_t *texbuf, clutbuffer_t *clutbuf, texture_t *texture, clut_t *clut) {
    // Check if this is a CLUT texture
    if (texture->psm == GS_PSM_4 || texture->psm == GS_PSM_8) {
        // allocate video memory for the texture (indexed format uses less space)
        texbuf->width = texture->width;
        texbuf->psm = texture->psm;
        texbuf->address = graph_vram_allocate(texture->width, texture->height, texbuf->psm, GRAPH_ALIGN_BLOCK);
        
		clutbuf->start = 0;
		clutbuf->storage_mode = CLUT_STORAGE_MODE1;
		clutbuf->load_method = CLUT_LOAD;
		clutbuf->psm = GS_PSM_32;

		int clut_width = texture->psm == GS_PSM_4 ? 8 : 16;
		int clut_height = texture->psm == GS_PSM_4 ? 2 : 16;

        // Allocate VRAM for the CLUT
        clutbuf->address = graph_vram_allocate(clut_width, clut_height, clutbuf->psm, GRAPH_ALIGN_BLOCK);
        
        packet_t *packet = packet_init(50, PACKET_NORMAL);
        qword_t *q = packet->data;
        
        // Transfer texture data (indexed pixels)
        q = draw_texture_transfer(q, texture->texture_data, texture->width, texture->height, 
                                  texbuf->psm, texbuf->address, texbuf->width);
        
        // Transfer CLUT data
        q = draw_texture_transfer(q, clut->palette, clut_width, clut_height,
                                 GS_PSM_32, clutbuf->address, 64);
        
        q = draw_texture_flush(q);
        
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
        dma_wait_fast();
        packet_free(packet);
    } else {
        texbuf->width = texture->width;
        texbuf->psm = texture->psm;
        texbuf->address = graph_vram_allocate(texture->width, texture->height, texbuf->psm, GRAPH_ALIGN_BLOCK);
        
		clutbuf->address = 0;
		clutbuf->start = 0;
		clutbuf->storage_mode = CLUT_STORAGE_MODE1;
		clutbuf->load_method = CLUT_NO_LOAD;
		clutbuf->psm = 0;

        packet_t *packet = packet_init(50, PACKET_NORMAL);
        qword_t *q = packet->data;
        
        q = draw_texture_transfer(q, texture->texture_data, texture->width, texture->height, 
                                 texbuf->psm, texbuf->address, texbuf->width);
        
        q = draw_texture_flush(q);
        
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
        dma_wait_fast();
        packet_free(packet);
        
	}
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

	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

	dma_wait_fast();

	packet_free(packet);
}

void flip_frame_buffer(packet_t *flip,framebuffer_t *frame)
{

	qword_t *q = flip->data;

	q = draw_framebuffer(q, 0, frame);
	q = draw_finish(q);

	dma_wait_fast();
	dma_channel_send_normal_ucab(DMA_CHANNEL_GIF, flip->data, q - flip->data, 0);

	draw_wait_finish();

}


typedef struct {
	VECTOR position;
	VECTOR rotation;
	VECTOR scale;
	prim_t prim;
	color_t rgbaq;
	vertex_f_t *vertices;
	xyz_t *screen_verts;
	color_t *colors;
	texel_t *st;
	mesh_t *mesh;
} __attribute__((aligned(16))) render_object_t;


typedef struct
{
	VECTOR position;
	VECTOR rotation;
	float fov;
	float near;
	float far;
	float aspect;
} camera_t;




int allocate_render_object(render_object_t *obj, mesh_t *mesh) {
	obj->vertices = memalign(128, sizeof(vertex_f_t) * mesh->vertex_count);
	obj->screen_verts = memalign(128, sizeof(xyz_t) * mesh->vertex_count);
	obj->colors = memalign(128, sizeof(color_t) * mesh->vertex_count);
	obj->st = memalign(128, sizeof(texel_t) * mesh->vertex_count);

	if (!obj->vertices || !obj->screen_verts || !obj->colors || !obj->st) {
		return -1;
	}

	obj->mesh = mesh;

	return 0;
}

qword_t *render_object(qword_t *q, MATRIX view_screen, render_object_t *obj, camera_t *cam) {

	MATRIX local_world, world_view, local_screen;

	qword_t *dmatag = q;
	q++;


	create_local_world(local_world, obj->position, obj->rotation);
	matrix_scale(local_world, local_world, obj->scale);

	create_world_view(world_view, cam->position, cam->rotation);

	create_local_screen(local_screen, local_world, world_view, view_screen);

	calculate_vertices((VECTOR*)obj->vertices, obj->mesh->vertex_count, (VECTOR*)obj->mesh->vertices, local_screen);

	draw_convert_xyz(obj->screen_verts, 2048, 2048, 16, obj->mesh->vertex_count, obj->vertices);

	draw_convert_rgbq(obj->colors, obj->mesh->vertex_count, obj->vertices, obj->mesh->colors, 255);

	draw_convert_st(obj->st, obj->mesh->vertex_count, obj->vertices, obj->mesh->texcoords);


	// Process each triangle strip separately
	int strip_idx;
	int vertex_idx;
	for (strip_idx = 0; strip_idx < obj->mesh->strip_count; strip_idx++) {
		triangle_strip_t* strip = &obj->mesh->strips[strip_idx];
		
		// Start a new primitive for each strip
		u64 *dw = (u64*)draw_prim_start(q, 0, &obj->prim, &obj->rgbaq);
		
		// Add all vertices in this strip
		for (int i = 0; i < strip->length; i++) {
			vertex_idx = strip->indices[i];
			*dw++ = obj->st[vertex_idx].uv;
			*dw++ = obj->colors[vertex_idx].rgbaq;
			*dw++ = obj->screen_verts[vertex_idx].xyz;
		}

		// Pad to qword alignment if necessary
		if ((3 * strip->length) & 1) {
			*dw++ = 0;
		}

		// End this strip
		q = draw_prim_end((qword_t*)dw, 3, DRAW_STQ2_REGLIST);
	}

	DMATAG_CNT(dmatag,q-dmatag-1,0,0,0);

	return q;
}



int draw(framebuffer_t *framebuf, zbuffer_t *zbuf, mesh_t *mesh) {
    
	VECTOR pos = {0.0f, 0.0f, 0.0f, 1.0f};
	VECTOR rot = {0.0f, 0.0f, 0.0f, 1.0f};
	VECTOR scale = {1.0f, 1.0f, 1.0f, 1.0f};

	VECTOR cam_pos = {0.0f, 0.0f, 80.0f, 1.0f};
	VECTOR cam_rot = {0.0f, 0.0f, 0.0f, 1.0f};

    int ctx = 0;

    packet_t *packets[2];
    packet_t *current;
    qword_t *dmatag;
            
    packets[0] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);
    packets[1] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);

    MATRIX view_screen;

	render_object_t obj;
	camera_t cam;

    prim_t prim;

    prim.type = PRIM_TRIANGLE_STRIP;
    prim.shading = PRIM_SHADE_GOURAUD;
    prim.mapping = DRAW_ENABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_ENABLE;
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

    packet_t *flip = packet_init(3, PACKET_UCAB);

	allocate_render_object(&obj, mesh);

	vector_copy(obj.position, pos);
	vector_copy(obj.rotation, rot);
	vector_copy(obj.scale, scale);
	obj.prim = prim;
	obj.rgbaq = color;

	cam.aspect = graph_aspect_ratio();
	cam.fov = 60.0f;
	cam.near = 1.0f;
	cam.far = 2000.0f;
	vector_copy(cam.position, cam_pos);
	vector_copy(cam.rotation, cam_rot);

	float tanHalfFovY = tanf(cam.fov * (PI / 180.0f) / 2.0f);
	float top = cam.near * tanHalfFovY;
	float bottom = -top;
	float right = top * cam.aspect;
	float left = -right;

    create_view_screen(view_screen, graph_aspect_ratio(), left, right, bottom, top, cam.near, cam.far);


    dma_wait_fast();

	qword_t *q;
    while (1)
    {
        current = packets[ctx];
        dmatag = current->data;

		q = current->data;

		dmatag = q;
		q++;

		// Clear framebuffer without any pixel testing.
		q = draw_disable_tests(q, 0, zbuf);
		q = draw_clear(q, 0, 2048.0f-(SCREEN_WIDTH/2), 2048.0f-(SCREEN_HEIGHT/2), framebuf[ctx].width, framebuf[ctx].height, 0x00,0x00,0x00);
		q = draw_enable_tests(q,0,zbuf);

		DMATAG_CNT(dmatag, q-dmatag - 1, 0, 0, 0);

		obj.rotation[1] += 0.01f;

		q = render_object(q, view_screen, &obj, &cam);

		dmatag = q;
		q++;

        q = draw_finish(q);

        DMATAG_END(dmatag,q-dmatag-1,0,0,0);

        dma_wait_fast();
        dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);

        draw_wait_finish();
        graph_wait_vsync();

        graph_set_framebuffer_filtered(framebuf[ctx].address, framebuf[ctx].width, framebuf[ctx].psm, 0, 0);

        ctx ^= 1;

        flip_frame_buffer(flip, &framebuf[ctx]);
    }

    packet_free(packets[0]);
    packet_free(packets[1]);

    return 0;
}

int main(void) {

	printf("Initializing DMAC GIF channel...\n");

	dma_channel_initialize(DMA_CHANNEL_GIF,NULL,0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	printf("Initializing GS...\n");

	framebuffer_t framebuf[2];
	zbuffer_t zbuf;
	texbuffer_t texbuf;
	clutbuffer_t clutbuf;

	init_gs(framebuf, &zbuf, &texbuf);

	printf("Loading model...\n");
	mesh_t *mesh = load_model(MESH_FILE);

	
	if (!mesh) {
		printf("FATAL: Failed to load model\n");
		return -1;
	}

	printf("Loading texture...\n");

	clut_t *clut = load_clut(CLUT_FILE);

	load_texture(&texbuf, &clutbuf, mesh->texture, clut);
	setup_texture(&texbuf, &clutbuf);

	printf("Drawing...\n");

	draw(framebuf, &zbuf, mesh);

	free_model(mesh);

	return 0;
}