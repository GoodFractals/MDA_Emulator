// ================================================================
//  MDA-A60Y1 Emulator — Scheduler
//
//  Initializes the HAL and the controller, then drives the
//  controller's tick function at a fixed 20 ms cadence. This is
//  the only file in the project that has a main() function.
//
//  Design intent:
//    - Thin scheduler. No logic lives here.
//    - Non-blocking. The controller must remain responsive to
//      fault conditions at all times.
//    - Fixed 20 ms tick. Fast enough to catch a 0.75 s preheat
//      transition, slow enough to leave cycles for the ADC.
// ================================================================

#include "pico/stdlib.h"
#include "mda_hal.h"
#include "mda_controller.h"

// Re-enable strict warnings that were disabled globally to silence
// SDK noise. These apply only to this file.
#if defined(__GNUC__)
    #pragma GCC diagnostic warning "-Wmissing-prototypes"
    #pragma GCC diagnostic warning "-Wundef"
#endif

int main(void)
{
    Hal_Init();
    Mda_Init();

    // Power of 10 Rule 2: constant-bound main loop.
    // The controller never blocks; each iteration yields 20 ms.
    while (true)
    {
        Mda_Tick();
        sleep_ms(20U);
    }

    // MISRA-C 15.5: single point of exit. Unreachable, but present.
    return 0;
}