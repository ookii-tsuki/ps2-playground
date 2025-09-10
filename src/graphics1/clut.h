#ifndef CLUT_H
#define CLUT_H

#include <tamtypes.h>
#include <gs_psm.h>

typedef struct {
    u32 id;             // Unique identifier for this CLUT
    u32* palette;       // RGBA color data in PS2 format
    u16 color_count;    // Number of colors (16 for GS_PSM_4, 256 for GS_PSM_8)
    u8 psm;             // GS_PSM_4 (0x14) or GS_PSM_8 (0x13)
} __attribute__((packed, aligned(16))) clut_t;

clut_t* load_clut(const char* filename);
void free_clut(clut_t* clut);
void print_clut_info(const clut_t* clut);

#endif