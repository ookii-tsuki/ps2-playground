#include "clut.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

clut_t* load_clut(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Failed to open CLUT file: %s\n", filename);
        return NULL;
    }
    
    // Read and verify magic identifier
    char magic[4];
    if (fread(magic, 1, 4, file) != 4 || 
        magic[0] != 'C' || magic[1] != 'L' || magic[2] != 'T' || magic[3] != '\0') {
        printf("Error: Invalid CLUT file format (bad magic number)\n");
        fclose(file);
        return NULL;
    }
    
    // Read PSM
    u8 psm;
    if (fread(&psm, 1, 1, file) != 1 || (psm != GS_PSM_4 && psm != GS_PSM_8)) {
        printf("Error: Invalid CLUT PSM (must be GS_PSM_4 or GS_PSM_8)\n");
        fclose(file);
        return NULL;
    }
    
    // Read color count
    u16 color_count;
    if (fread(&color_count, 2, 1, file) != 1) {
        printf("Error: Failed to read CLUT color count\n");
        fclose(file);
        return NULL;
    }
    
    // Read unique CLUT identifier
    u32 clut_id;
    if (fread(&clut_id, 4, 1, file) != 1) {
        printf("Error: Failed to read CLUT identifier\n");
        fclose(file);
        return NULL;
    }
    
    // Validate color count based on PSM
    u16 max_colors = (psm == GS_PSM_4) ? 16 : 256;
    if (color_count > max_colors) {
        printf("Warning: CLUT color count (%d) exceeds maximum for PSM 0x%02X (%d). Truncating.\n",
               color_count, psm, max_colors);
        color_count = max_colors;
    }
    
    clut_t* clut = (clut_t*)memalign(16, sizeof(clut_t));
    if (!clut) {
        printf("Error: Failed to allocate memory for CLUT structure\n");
        fclose(file);
        return NULL;
    }
    
    // Initialize the CLUT
    clut->psm = psm;
    clut->color_count = color_count;
    clut->id = clut_id;  // Store the unique identifier
    
    // Calculate size and allocate palette data with 16-byte alignment
    unsigned int palette_size = color_count * sizeof(u32);
    clut->palette = (u32*)memalign(128, palette_size);
    if (!clut->palette) {
        printf("Error: Failed to allocate memory for palette data\n");
        free(clut);
        fclose(file);
        return NULL;
    }
    
    // Read color data (RGBA format)
    unsigned char color_bytes[4];
    for (int i = 0; i < color_count; i++) {
        if (fread(color_bytes, 1, 4, file) != 4) {
            printf("Error: Failed to read CLUT color data at index %d\n", i);
            free(clut->palette);
            free(clut);
            fclose(file);
            return NULL;
        }
        
        // Convert RGBA bytes to PS2 RGBA format (same order in memory)
        // NOTE: PS2 GS uses RGBA in memory but ABGR in register format
        clut->palette[i] = (color_bytes[3] << 24) | (color_bytes[2] << 16) | 
                           (color_bytes[1] << 8) | color_bytes[0];
    }
    
    fclose(file);
    printf("Successfully loaded CLUT: PSM=0x%02X, %d colors\n", psm, color_count);
    print_clut_info(clut);
    return clut;
}

void free_clut(clut_t* clut) {
    if (clut) {
        if (clut->palette) {
            free(clut->palette);
        }
        free(clut);
    }
}

void print_clut_info(const clut_t* clut) {
    if (!clut) {
        printf("Error: NULL CLUT pointer\n");
        return;
    }
    
    printf("CLUT Information:\n");
    printf("================\n");
    printf("ID: 0x%08X\n", clut->id);  // Display the CLUT ID
    printf("PSM: 0x%02X (%s)\n", clut->psm, 
           clut->psm == GS_PSM_4 ? "4-bit indexed" : 
           clut->psm == GS_PSM_8 ? "8-bit indexed" : "Unknown");
    printf("Color Count: %d colors\n", clut->color_count);
    
    // Print the first few colors as RGBA hex values
    printf("First 8 colors (RGBA format):\n");
    int count = clut->color_count < 8 ? clut->color_count : 8;
    for (int i = 0; i < count; i++) {
        u32 color = clut->palette[i];
        printf("  Color %d: #%02X%02X%02X%02X\n", i,
               (color & 0xFF),           // R
               ((color >> 8) & 0xFF),    // G
               ((color >> 16) & 0xFF),   // B
               ((color >> 24) & 0xFF));  // A
    }
    printf("\n");
}