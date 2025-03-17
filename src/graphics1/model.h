#ifndef MODEL_H
#define MODEL_H

#include <draw_types.h>

typedef struct {
    int width;
    int height;
    int psm;
    int texture_size; // Size of the texture data in bytes
    void* texture_data; // Pointer to raw texture data
} __attribute__((packed,aligned(16))) texture_t;

typedef struct {
    int index_count;
    int vertex_count;
    int color_count;
    int texcoord_count;
    int* indices;            // Array of indices
    vertex_f_t* vertices;     // Array of vertices
    color_f_t* colors;        // Array of colors
    texel_f_t* texcoords;     // Array of texture coordinates
    texture_t* texture;       // Pointer to texture data
} __attribute__((packed,aligned(16))) mesh_t;

mesh_t* load_model(const char* filename);
void free_model(mesh_t* model);
void print_mesh_data(const mesh_t* model);

#endif