#ifndef PAD_H
#define PAD_H

#include <tamtypes.h>

// Button definitions
#define PAD_SELECT      0x0001
#define PAD_L3          0x0002
#define PAD_R3          0x0004
#define PAD_START       0x0008
#define PAD_UP          0x0010
#define PAD_RIGHT       0x0020
#define PAD_DOWN        0x0040
#define PAD_LEFT        0x0080
#define PAD_L2          0x0100
#define PAD_R2          0x0200
#define PAD_L1          0x0400
#define PAD_R1          0x0800
#define PAD_TRIANGLE    0x1000
#define PAD_CIRCLE      0x2000
#define PAD_CROSS       0x4000
#define PAD_SQUARE      0x8000

// Axis definitions
typedef enum {
    AXIS_LEFT_X,
    AXIS_LEFT_Y,
    AXIS_RIGHT_X,
    AXIS_RIGHT_Y
} PadAxis;

// Initialize the pad system
int pad_init(void);

// Update pad states - call this once per frame
void pad_update(void);

// Check if a button is currently pressed down
int pad_get_button(int port, u32 button);

// Check if a button was pressed this frame
int pad_get_button_down(int port, u32 button);

// Check if a button was released this frame
int pad_get_button_up(int port, u32 button);

// Get analog stick axis value normalized to -1.0 to 1.0 range
float pad_get_axis(int port, PadAxis axis);

// Check if a controller is connected
int pad_is_connected(int port);

// Set rumble (if supported) - value range 0-1
void pad_set_rumble(int port, float intensity);

#endif // PAD_H