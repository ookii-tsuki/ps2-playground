#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <tamtypes.h>
#include <malloc.h>
#include <gs_psm.h>

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
    model->texcoords = NULL;
    model->texture = NULL;

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
    
    // Read texture coordinates (texcoords)
    fscanf(file, "%d", &model->texcoord_count);
    if (model->texcoord_count > 0) {
        model->texcoords = (texel_f_t*)memalign(16, model->texcoord_count * sizeof(texel_f_t));
        if (!model->texcoords) {
            perror("Failed to allocate memory for texture coordinates");
            free_model(model);
            fclose(file);
            return NULL;
        }
        
        for (int i = 0; i < model->texcoord_count; i++) {
            if (fscanf(file, "%f,%f,%f,%f", &model->texcoords[i].s, &model->texcoords[i].t, 
                      &model->texcoords[i].r, &model->texcoords[i].q) != 4) {
                printf("Error: Failed to read texture coordinate data\n");
                free_model(model);
                fclose(file);
                return NULL;
            }
        }
    } else {
        model->texcoords = NULL;
    }
    
    // Read texture data if present
    // Try to read texture dimensions and format
    int width, height, psm, texture_size;
    if (fscanf(file, "%d,%d,%d,%d", &width, &height, &psm, &texture_size) == 4) {
        // Texture data is present
        model->texture = (texture_t*)memalign(16, sizeof(texture_t));
        if (!model->texture) {
            perror("Failed to allocate memory for texture structure");
            free_model(model);
            fclose(file);
            return NULL;
        }
        
        model->texture->width = width;
        model->texture->height = height;
        model->texture->psm = psm;
        model->texture->texture_size = texture_size;
        
        // Allocate memory for texture data
        model->texture->texture_data = memalign(16, texture_size);
        if (!model->texture->texture_data) {
            perror("Failed to allocate memory for texture data");
            free_model(model);
            fclose(file);
            return NULL;
        }
        
        // Read texture data depending on the PSM format
        if (psm == GS_PSM_32) { // GS_PSM_32 (32 bits per pixel)
            unsigned int pixel;
            unsigned char* data = (unsigned char*)model->texture->texture_data;
            int pos = 0;
            
            for (int i = 0; i < width * height; i++) {
                if (fscanf(file, "%x", &pixel) != 1) {
                    printf("Error: Failed to read texture data (32-bit format)\n");
                    free_model(model);
                    fclose(file);
                    return NULL;
                }
                
                // Store RGBA components
                data[pos++] = (pixel >> 16) & 0xFF;  // R
                data[pos++] = (pixel >> 8) & 0xFF;   // G
                data[pos++] = pixel & 0xFF;          // B
                data[pos++] = (pixel >> 24) & 0xFF;  // A
            }
        } else if (psm == GS_PSM_24) { // GS_PSM_24 (24 bits per pixel)
            unsigned int pixel;
            unsigned char* data = (unsigned char*)model->texture->texture_data;
            int pos = 0;
            
            for (int i = 0; i < width * height; i++) {
                if (fscanf(file, "%x", &pixel) != 1) {
                    printf("Error: Failed to read texture data (24-bit format)\n");
                    free_model(model);
                    fclose(file);
                    return NULL;
                }
                
                // Store RGB components (no alpha)
                data[pos++] = (pixel >> 16) & 0xFF;  // R
                data[pos++] = (pixel >> 8) & 0xFF;   // G
                data[pos++] = pixel & 0xFF;          // B
            }
        } else if (psm == GS_PSM_16 || psm == GS_PSM_16S) { // GS_PSM_16 or GS_PSM_16S (16 bits per pixel)
            unsigned short pixel;
            unsigned short* data = (unsigned short*)model->texture->texture_data;
            
            for (int i = 0; i < width * height; i++) {
                if (fscanf(file, "%hx", &pixel) != 1) {
                    printf("Error: Failed to read texture data (16-bit format)\n");
                    free_model(model);
                    fclose(file);
                    return NULL;
                }
                
                data[i] = pixel;
            }
        } else {
            printf("Warning: Unsupported texture format (PSM = %d). Skipping texture data.\n", psm);
            free(model->texture->texture_data);
            free(model->texture);
            model->texture = NULL;
        }
    } else {
        // No texture data
        model->texture = NULL;
    }

    print_mesh_data(model);

    fclose(file);
    return model;
}

void free_model(mesh_t* model) {
    if (model) {
        if (model->indices) free(model->indices);
        if (model->vertices) free(model->vertices);
        if (model->colors) free(model->colors);
        if (model->texcoords) free(model->texcoords);
        if (model->texture) {
            if (model->texture->texture_data) free(model->texture->texture_data);
            free(model->texture);
        }
        free(model);
    }
}

void print_mesh_data(const mesh_t* model) {
    if (!model) {
        printf("Error: NULL model pointer\n");
        return;
    }
    
    printf("Mesh Data Summary:\n");
    printf("=================\n");
    printf("Indices: %d\n", model->index_count);
    printf("Vertices: %d\n", model->vertex_count);
    printf("Colors: %d\n", model->color_count);
    printf("Texture Coordinates: %d\n", model->texcoord_count);
    
    // Print first few indices if available
    if (model->indices && model->index_count > 0) {
        printf("\nFirst 3 triangles (indices):\n");
        int count = model->index_count < 9 ? model->index_count : 9;
        for (int i = 0; i < count; i += 3) {
            printf("  Triangle %d: [%d, %d, %d]\n", 
                  i/3, model->indices[i], model->indices[i+1], model->indices[i+2]);
        }
    }
    
    // Print first few vertices if available
    if (model->vertices && model->vertex_count > 0) {
        printf("\nFirst 3 vertices:\n");
        int count = model->vertex_count < 3 ? model->vertex_count : 3;
        for (int i = 0; i < count; i++) {
            printf("  Vertex %d: (%.2f, %.2f, %.2f, %.2f)\n", i,
                  model->vertices[i].x, model->vertices[i].y, 
                  model->vertices[i].z, model->vertices[i].w);
        }
    }
    
    // Print first few colors if available
    if (model->colors && model->color_count > 0) {
        printf("\nFirst 3 colors:\n");
        int count = model->color_count < 3 ? model->color_count : 3;
        for (int i = 0; i < count; i++) {
            printf("  Color %d: (R:%.2f, G:%.2f, B:%.2f, A:%.2f)\n", i,
                  model->colors[i].r, model->colors[i].g, 
                  model->colors[i].b, model->colors[i].a);
        }
    }
    
    // Print first few texture coordinates if available
    if (model->texcoords && model->texcoord_count > 0) {
        printf("\nFirst 3 texture coordinates:\n");
        int count = model->texcoord_count < 3 ? model->texcoord_count : 3;
        for (int i = 0; i < count; i++) {
            printf("  TexCoord %d: (S:%.2f, T:%.2f, R:%.2f, Q:%.2f)\n", i,
                  model->texcoords[i].s, model->texcoords[i].t, 
                  model->texcoords[i].r, model->texcoords[i].q);
        }
    }
    
    // Print texture information if available
    if (model->texture) {
        printf("\nTexture Information:\n");
        printf("  Dimensions: %d x %d\n", model->texture->width, model->texture->height);
        printf("  Format (PSM): %d\n", model->texture->psm);
        printf("  Data Size: %d bytes\n", model->texture->texture_size);
        
        // Optional: Print a few bytes of texture data as hex
        if (model->texture->texture_data) {
            printf("  First 16 bytes of texture data: ");
            unsigned char* data = (unsigned char*)model->texture->texture_data;
            int bytes_to_show = model->texture->texture_size < 16 ? model->texture->texture_size : 16;
            for (int i = 0; i < bytes_to_show; i++) {
                printf("%02X ", data[i]);
            }
            printf("\n");
        } else {
            printf("  No texture data available\n");
        }
    } else {
        printf("\nNo texture information available\n");
    }
    
    printf("\n=================\n");
}