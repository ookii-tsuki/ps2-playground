#ifndef MODEL_H
#define MODEL_H

#include <draw_types.h>

typedef struct {
    int width;
    int height;
    int psm;
    int texture_size; // Size of the texture data in bytes
    void* texture_data; // Pointer to raw texture data
} __attribute__((packed,aligned(8))) texture_t;

typedef struct {
    int length;     // Number of indices in this strip
    int* indices;   // Array of indices for this strip
} __attribute__((packed,aligned(8))) triangle_strip_t;

typedef struct {
    int strip_count;        // Number of triangle strips
    triangle_strip_t* strips; // Array of triangle strips
    int vertex_count;
    int color_count;
    int texcoord_count;
    vertex_f_t* vertices;     // Array of vertices
    color_f_t* colors;        // Array of colors
    texel_f_t* texcoords;     // Array of texture coordinates
    texture_t* texture;       // Pointer to texture data
} __attribute__((packed,aligned(16))) mesh_t;

mesh_t* load_model(const char* filename);
void free_model(mesh_t* model);
void print_mesh_data(const mesh_t* model);

#endif