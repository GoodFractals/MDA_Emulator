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
#include "pico/cyw43_arch.h" // <-- Add this
#include "mda_hal.h"
#include "mda_controller.h"

int main(void)
{
    if (cyw43_arch_init()) { 
        return -1; 
    }
    
    // Turn the LED on SOLID and leave it on
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    Hal_Init();
    Mda_Init();

    while (true)
    {
        Mda_Tick();
        sleep_ms(20U);
    }

    return 0;
}