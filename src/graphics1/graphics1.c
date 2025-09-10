#include <stdio.h>
#include <gs_psm.h>
#include <graph.h>
#include <kernel.h>
#include <tamtypes.h>
#include <debug.h>
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
#include <texture_manager.h>
#include <pad.h>

#define PI 3.1415926

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

#define DRAWBUF_SIZE 20000


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
	texbuffer_t *texbuf;
	clutbuffer_t *clutbuf;
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


static render_object_t **objects;
static int num_objects;

static render_object_t *current_object;
static int current_object_index;

int init_gs(framebuffer_t *framebuf, zbuffer_t *zbuf) {
	
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
	zbuf->zsm = GS_ZBUF_24;
	zbuf->address = graph_vram_allocate(framebuf->width, framebuf->height, zbuf->zsm, GRAPH_ALIGN_PAGE);

	// set GS mode
	graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_DISABLE);
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



void flip_frame_buffer(packet_t *flip,framebuffer_t *frame)
{

	qword_t *q = flip->data;

	q = draw_framebuffer(q, 0, frame);
	q = draw_finish(q);

	dma_wait_fast();
	dma_channel_send_normal_ucab(DMA_CHANNEL_GIF, flip->data, q - flip->data, 0);

	draw_wait_finish();

}




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

int load_models(const char **filenames, int count) {
	objects = malloc(sizeof(render_object_t*) * count);

	if (!objects) {
		return -1;
	}

	VECTOR pos = {0.0f, -1.0f, 0.0f, 1.0f};
	VECTOR rot = {0.0f, 0.0f, 0.0f, 1.0f};
	VECTOR scale = {1.0f, 1.0f, 1.0f, 1.0f};

	prim_t prim;

    prim.type = PRIM_TRIANGLE_STRIP;
    prim.shading = PRIM_SHADE_GOURAUD;
    prim.mapping = DRAW_ENABLE;
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


	for (int i = 0; i < count; i++) {
		objects[i] = malloc(sizeof(render_object_t));

		if (!objects[i]) {
			return -1;
		}

		objects[i]->mesh = load_model(filenames[i]);

		if (!objects[i]->mesh) {
			return -1;
		}

		if (allocate_render_object(objects[i], objects[i]->mesh) < 0) {
			return -1;
		}

		vector_copy(objects[i]->position, pos);
		vector_copy(objects[i]->rotation, rot);
		vector_copy(objects[i]->scale, scale);
		objects[i]->prim = prim;
		objects[i]->rgbaq = color;

	}

	return 0;
}


int load_cluts(const char **filenames, int count) {

	for (int i = 0; i < count; i++) {

		clut_t *clut = load_clut(filenames[i]);

		if (!clut) {
			return -1;
		}

		clutbuffer_t *clutbuf = load_clut_in_vram(clut);

		u32 clut_id = clut->id;
		
		for (int j = 0; j < num_objects; j++)
		{

			if (objects[j]->mesh->texture->clut_id == clut_id)
			{
				objects[j]->clutbuf = clutbuf;
			}
		}

	}

	return 0;
}

void set_current_object(int i) {

	if (current_object) {
		unload_texture_from_vram(current_object->texbuf);
	}

	current_object = objects[i];

	printf("ahh");
	texbuffer_t *texbuf = load_texture_in_vram(current_object->mesh->texture);

	current_object->texbuf = texbuf;

	printf("texbuf: %08X\n", (u32)current_object->texbuf->address);

	clutbuffer_t *clutbuf = current_object->mesh->texture->clut_id > 0 ? current_object->clutbuf : &no_clut;
	printf("clutbuf: %08X\n", (u32)clutbuf->address);

	setup_texture(current_object->texbuf, clutbuf);

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

	draw_convert_xyz(obj->screen_verts, 2048, 2048, 24, obj->mesh->vertex_count, obj->vertices);

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


int draw(framebuffer_t *framebuf, zbuffer_t *zbuf) {

	VECTOR cam_pos = {0.0f, 0.0f, 20.0f, 1.0f};
	VECTOR cam_rot = {0.0f, 0.0f, 0.0f, 1.0f};

    int ctx = 0;

    packet_t *packets[2];
    packet_t *current;
    qword_t *dmatag;
            
    packets[0] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);
    packets[1] = packet_init(DRAWBUF_SIZE, PACKET_NORMAL);

    MATRIX view_screen;

	camera_t cam;

    packet_t *flip = packet_init(3, PACKET_UCAB);



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
		pad_update();


		if (pad_get_button_down(0, PAD_R1)) {
			current_object_index++;
			if (current_object_index >= num_objects) {
				current_object_index = 0;
			}
			printf("Current object: %d\n", current_object_index);
			
			set_current_object(current_object_index);
		}


		float pad_left_x = pad_get_axis(0, AXIS_LEFT_X);

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

		current_object->rotation[1] += 0.04f * pad_left_x;

		q = render_object(q, view_screen, current_object, &cam);

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

	init_gs(framebuf, &zbuf);

	// Initialize pad
	printf("Initializing pad...\n");
	if (pad_init() < 0) {
		printf("FATAL: Failed to initialize pad\n");
		return -1;
	}

	printf("Loading models...\n");
	const char *model_filenames[] = {
		"host:/spyro.bin",
		"host:/kratos.bin",
		//"host:/oiia.bin"
	};

	num_objects = sizeof(model_filenames) / sizeof(model_filenames[0]);
	
	int l = load_models(model_filenames, num_objects);
	
	if (l < 0) {
		printf("FATAL: Failed to load models\n");
		return -1;
	}

	printf("Loading texture...\n");

	const char *clut_filenames[] = {
		"host:/spyro.clt",
		"host:/kratos.clt",
	};

	l = load_cluts(clut_filenames, sizeof(clut_filenames) / sizeof(clut_filenames[0]));

	if (l < 0) {
		printf("FATAL: Failed to load CLUTs\n");
		return -1;
	}

	printf("Drawing...\n");

	current_object_index = 0;
	set_current_object(current_object_index);

	draw(framebuf, &zbuf);


	return 0;
}