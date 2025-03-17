#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <tamtypes.h>
#include <malloc.h>

mesh_t* load_model(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    mesh_t* model = (mesh_t*)memalign(16, sizeof(mesh_t));
    if (!model) {
        perror("Failed to allocate memory for model");
        fclose(file);
        return NULL;
    }

    // Initialize pointers to NULL to handle errors safely
    model->indices = NULL;
    model->vertices = NULL;
    model->colors = NULL;

    fscanf(file, "%d", &model->index_count);
    if (model->index_count <= 0) {
        printf("Error: Invalid triangle count in model file\n");
        free_model(model);
        fclose(file);
        return NULL;
    }

    model->indices = (int*)memalign(16, model->index_count * sizeof(int));
    if (!model->indices) {
        perror("Failed to allocate memory for indices");
        free_model(model);
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < model->index_count; i += 3) {
        if (fscanf(file, "%d,%d,%d", &model->indices[i], &model->indices[i + 1], &model->indices[i + 2]) != 3) {
            printf("Error: Failed to read triangle indices\n");
            free_model(model);
            fclose(file);
            return NULL;
        }
    }

    fscanf(file, "%d", &model->vertex_count);
    if (model->vertex_count <= 0) {
        printf("Error: Invalid vertex count in model file\n");
        free_model(model);
        fclose(file);
        return NULL;
    }

    model->vertices = (vertex_f_t*)memalign(16, model->vertex_count * sizeof(vertex_f_t));
    if (!model->vertices) {
        perror("Failed to allocate memory for vertices");
        free_model(model);
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < model->vertex_count; i++) {
        if (fscanf(file, "%f,%f,%f,%f", &model->vertices[i].x, &model->vertices[i].y, 
                  &model->vertices[i].z, &model->vertices[i].w) != 4) {
            printf("Error: Failed to read vertex data\n");
            free_model(model);
            fclose(file);
            return NULL;
        }
    }

    fscanf(file, "%d", &model->color_count);
    
    // Check if color count matches vertex count
    if (model->color_count != model->vertex_count) {
        printf("Warning: Color count (%d) doesn't match vertex count (%d). Using default color.\n", 
               model->color_count, model->vertex_count);
        
        // Use vertex count for colors and set default grayish color
        model->color_count = model->vertex_count;
        model->colors = (color_f_t*)memalign(16, model->color_count * sizeof(color_f_t));
        if (!model->colors) {
            perror("Failed to allocate memory for colors");
            free_model(model);
            fclose(file);
            return NULL;
        }
        
        // Set default grayish color for all vertices
        for (int i = 0; i < model->color_count; i++) {
            model->colors[i].r = 0.7f;
            model->colors[i].g = 0.7f;
            model->colors[i].b = 0.7f;
            model->colors[i].a = 1.0f;
        }
    } else {
        // Normal case: color count matches vertex count
        model->colors = (color_f_t*)memalign(16, model->color_count * sizeof(color_f_t));
        if (!model->colors) {
            perror("Failed to allocate memory for colors");
            free_model(model);
            fclose(file);
            return NULL;
        }
        
        for (int i = 0; i < model->color_count; i++) {
            if (fscanf(file, "%f,%f,%f,%f", &model->colors[i].r, &model->colors[i].g, 
                      &model->colors[i].b, &model->colors[i].a) != 4) {
                printf("Error: Failed to read color data\n");
                free_model(model);
                fclose(file);
                return NULL;
            }
        }
    }

    fclose(file);
    return model;
}

void free_model(mesh_t* model) {
    if (model) {
        free(model->indices);
        free(model->vertices);
        free(model->colors);
        free(model);
    }
}