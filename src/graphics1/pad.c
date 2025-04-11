#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pad.h"

// First, let's define the pad states we need to track
#define MAX_CONTROLLERS 2

typedef struct {
    u32 buttonsPressed;     // Buttons pressed this frame
    u32 buttonsReleased;    // Buttons released this frame
    u32 buttonsHeld;        // Buttons held down
    u32 previousButtons;    // Previous frame's buttons
    u8  leftStickX;         // Left analog stick X axis (0-255)
    u8  leftStickY;         // Left analog stick Y axis (0-255)
    u8  rightStickX;        // Right analog stick X axis (0-255)
    u8  rightStickY;        // Right analog stick Y axis (0-255)
    int     connected;          // Whether the controller is connected
    int     actuators;          // Number of actuators (for rumble)
} PadState;


// Module variables
static PadState padStates[MAX_CONTROLLERS];
static char padBuf[MAX_CONTROLLERS][256] __attribute__((aligned(64)));
static int initialized = 0;

// Forward declarations of internal functions
static void pad_wait(int port);
static int pad_get_state(int port);

// Initialize the pad system
int pad_init(void) {
    int ret;
    
    // Initialize the SIF RPC interface for controller communication
    SifInitRpc(0);
    
    // Load necessary modules for pad access
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        printf("Failed to load SIO2MAN module\n");
        return 0;
    }
    
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        printf("Failed to load PADMAN module\n");
        return 0;
    }
    
    // Initialize libpad
    padInit(0);
    
    // Setup pads for both ports
    padPortOpen(0, 0, padBuf[0]);
    padPortOpen(1, 0, padBuf[1]);
    
    // Clear the pad state structures
    memset(&padStates[0], 0, sizeof(PadState));
    memset(&padStates[1], 0, sizeof(PadState));
    
    initialized = 1;
    return 1;
}

// Update pad states - call this once per frame
void pad_update(void) {
    for (int port = 0; port < MAX_CONTROLLERS; port++) {
        struct padButtonStatus buttons;
        int state = pad_get_state(port);
        
        // If the controller is connected and ready
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
            // Get the pad data
            if (padRead(port, 0, &buttons) != 0) {
                // Store previous buttons to calculate pressed/released
                padStates[port].previousButtons = padStates[port].buttonsHeld;
                
                // Read current button state (inverted because 0 = pressed on PS2)
                padStates[port].buttonsHeld = ~buttons.btns & 0xffff;
                
                // Calculate buttons pressed this frame (weren't down last frame but are now)
                padStates[port].buttonsPressed = 
                    (padStates[port].buttonsHeld & ~padStates[port].previousButtons);
                    
                // Calculate buttons released this frame (were down last frame but not now)
                padStates[port].buttonsReleased = 
                    (~padStates[port].buttonsHeld & padStates[port].previousButtons);
                
                // Read analog sticks
                padStates[port].leftStickX = buttons.ljoy_h;
                padStates[port].leftStickY = buttons.ljoy_v;
                padStates[port].rightStickX = buttons.rjoy_h;
                padStates[port].rightStickY = buttons.rjoy_v;
                
                padStates[port].connected = 1;
            }
        } else {
            padStates[port].connected = 0;
        }
    }
}

// Check if a button is currently pressed down
int pad_get_button(int port, u32 button) {
    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
        return 0;
    
    return (padStates[port].buttonsHeld & button) != 0;
}

// Check if a button was pressed this frame
int pad_get_button_down(int port, u32 button) {
    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
        return 0;
    
    return (padStates[port].buttonsPressed & button) != 0;
}

// Check if a button was released this frame
int pad_get_button_up(int port, u32 button) {
    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
        return 0;
    
    return (padStates[port].buttonsReleased & button) != 0;
}

// Get analog stick axis value normalized to -1.0 to 1.0 range
float pad_get_axis(int port, PadAxis axis) {
    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
        return 0.0f;
    
    if (!padStates[port].connected)
        return 0.0f;
        
    u8 value = 0;
    switch (axis) {
        case AXIS_LEFT_X:
            value = padStates[port].leftStickX;
            break;
        case AXIS_LEFT_Y:
            value = padStates[port].leftStickY;
            break;
        case AXIS_RIGHT_X:
            value = padStates[port].rightStickX;
            break;
        case AXIS_RIGHT_Y:
            value = padStates[port].rightStickY;
            break;
        default:
            return 0.0f;
    }
    
    // Convert from 0-255 range to -1.0 to 1.0 range
    // 128 is the center position
    float value_normalized = ((float)value - 128.0f) / 128.0f;
    return (fabsf(value_normalized) < 0.1f) ? 0.0f : value_normalized;
}

// Check if a controller is connected
int pad_is_connected(int port) {
    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
        return 0;
    
    return padStates[port].connected;
}

// Set rumble (if supported) - value range 0-1
//void pad_set_rumble(int port, float intensity) {
//    if (port < 0 || port >= MAX_CONTROLLERS || !initialized) 
//        return;
//    
//    if (!padStates[port].connected || padStates[port].actuators < 1)
//        return;
//    
//    // Convert 0-1 to 0-255 range
//    u8 actuatorVal = (u8)(intensity * 255.0f);
//    
//    // Set actuator values
//    actuatorVal = actuatorVal > 0 ? 1 : 0; // Typically just on/off for PS2
//    padSetActDirect(port, 0, &actuatorVal);
//}

// Helper function to wait for the pad to be ready
static void pad_wait(int port) {
    int state, last_state;
    
    state = padGetState(port, 0);
    last_state = -1;
    
    while ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != last_state) {
            last_state = state;
        }
        state = padGetState(port, 0);
    }
}

// Get the current state of the pad
static int pad_get_state(int port) {
    int state = padGetState(port, 0);
    
    // If we have a stable connection, check for supported features
    if (state == PAD_STATE_STABLE) {
        // Check if this is the first time we're connected
        if (padStates[port].actuators == 0) {
            // Check actuators (for rumble support)
            padStates[port].actuators = padInfoAct(port, 0, -1, 0);
            
            // Set the pad to analog mode
            padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
            pad_wait(port);
        }
    } else {
        // Controller is disconnected
        padStates[port].actuators = 0;
    }
    
    return state;
}