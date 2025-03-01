#include <tamtypes.h>
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math3d.h>

#include <stdint.h>
typedef uint64_t u64;



int main(int argc, char *argv[]) {
    // Initialize DMA and GS
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_NONINTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->Dithering = GS_SETTING_OFF;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    float x = 0.0f;
    float y = 200.0f;
    float speed = 2.0f;

    while (1) {
        // Clear the screen
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

        // Draw a rectangle
        gsKit_prim_sprite(gsGlobal, x, y, x + 50, y + 50, 1, GS_SETREG_RGBAQ(0xFF, 0x00, 0x00, 0x80, 0x00));

        // Update the position
        x += speed;
        if (x > gsGlobal->Width) {
            x = 0.0f;
        }

        // Sync and flip the frame
        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }

    return 0;
}