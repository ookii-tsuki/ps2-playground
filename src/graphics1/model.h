#ifndef MODEL_H
#define MODEL_H

#include <draw_types.h>

typedef struct {
    int index_count;
    int vertex_count;
    int color_count;
    int* indices;            // Array of indices
    vertex_f_t* vertices;     // Array of vertices
    color_f_t* colors;        // Array of colors
} __attribute__((packed,aligned(16))) mesh_t;

mesh_t* load_model(const char* filename);
void free_model(mesh_t* model);

#endif