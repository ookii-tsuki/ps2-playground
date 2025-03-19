#ifndef CLUT_H
#define CLUT_H

#include <tamtypes.h>
#include <gs_psm.h>

typedef struct {
    u8 psm;             // GS_PSM_4 (0x14) or GS_PSM_8 (0x13)
    u16 color_count;    // Number of colors (16 for GS_PSM_4, 256 for GS_PSM_8)
    u32* palette;       // RGBA color data in PS2 format
} __attribute__((packed, aligned(8))) clut_t;

clut_t* load_clut(const char* filename);
void free_clut(clut_t* clut);
void print_clut_info(const clut_t* clut);

#endif