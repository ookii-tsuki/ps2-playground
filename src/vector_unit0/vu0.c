#include <stdio.h>
#include <tamtypes.h>
#include <sifrpc.h>
#include <debug.h>
#include <unistd.h>
#include <kernel.h>
#include <stdint.h>
typedef uint64_t u64;


float vu0_add(float a, float b) {
    float result;
    
    asm volatile(
        "add.s %0, %1, %2"    // Add single-precision floating point values
        : "=f"(result)        // Output: result goes into result variable
        : "f"(a), "f"(b)      // Inputs: a and b are floating point values
    );
    
    return result;
}

float vu0_max(float a, float b) {
    float result;
    
    asm volatile(
        "c.lt.s %1, %2        \n\t"  // Compare if a < b
        "bc1t 1f             \n\t"  // Branch if true
        "mov.s %0, %1        \n\t"  // Move a to result
        "j 2f                \n\t"  // Jump to end
        "1:                  \n\t"  // b is greater than a
        "mov.s %0, %2        \n\t"  // Move b to result
        "2:                  \n\t"  // end label
        : "=f"(result)              // Output
        : "f"(a), "f"(b)            // Inputs
    );
    
    return result;
}


int main(int argc, char *argv[]) {

    sceSifInitRpc(0);
    init_scr();

    scr_setXY(20, 20);

    float a = 1.0f;
    float b = 2.0f;

    float result = vu0_add(a, b);

    scr_printf("a + b = %f\n", result);

    scr_setXY(20, 22);

    result = vu0_max(a, b);

    scr_printf("max(a, b) = %f\n", result);

    sleep(5);

    return 0;
}