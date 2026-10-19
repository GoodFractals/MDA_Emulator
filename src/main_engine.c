// ================================================================
//  Engine Simulator — Application Firmware
//
//  Target:     Raspberry Pi Pico W (RP2040)
//  Purpose:    Reads MDA relay outputs and simulates engine sensors.
// ================================================================

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// Inputs from MDA
#define IN_START_RELAY 10
#define IN_FUEL_RELAY  11
#define IN_AUX_RELAY   12

// Outputs to MDA (Sensors)
#define OUT_L1_VOLTAGE  17
#define OUT_CHARGE_ALT  26
#define OUT_OIL_SWITCH  18
#define OUT_TEMP_SWITCH 19

// Local Debug LEDs
#define LED_CRANKING 15
#define LED_RUNNING  14
#define LED_FAULT    13

int main(void) {
    stdio_init_all();

    // Configure inputs from MDA (pull-downs)
    gpio_init(IN_START_RELAY); gpio_set_dir(IN_START_RELAY, GPIO_IN); gpio_pull_down(IN_START_RELAY);
    gpio_init(IN_FUEL_RELAY);  gpio_set_dir(IN_FUEL_RELAY, GPIO_IN);  gpio_pull_down(IN_FUEL_RELAY);
    gpio_init(IN_AUX_RELAY);   gpio_set_dir(IN_AUX_RELAY, GPIO_IN);   gpio_pull_down(IN_AUX_RELAY);

    // Configure outputs to MDA
    gpio_init(OUT_L1_VOLTAGE);  gpio_set_dir(OUT_L1_VOLTAGE, GPIO_OUT);
    gpio_init(OUT_CHARGE_ALT);  gpio_set_dir(OUT_CHARGE_ALT, GPIO_OUT);
    gpio_init(OUT_OIL_SWITCH);  gpio_set_dir(OUT_OIL_SWITCH, GPIO_OUT);
    gpio_init(OUT_TEMP_SWITCH); gpio_set_dir(OUT_TEMP_SWITCH, GPIO_OUT);

    // Configure LEDs
    gpio_init(LED_CRANKING); gpio_set_dir(LED_CRANKING, GPIO_OUT);
    gpio_init(LED_RUNNING);  gpio_set_dir(LED_RUNNING, GPIO_OUT);
    gpio_init(LED_FAULT);    gpio_set_dir(LED_FAULT, GPIO_OUT);

    // Initial state: Engine off, no faults
    gpio_put(OUT_L1_VOLTAGE, 0);
    gpio_put(OUT_CHARGE_ALT, 0);
    gpio_put(OUT_OIL_SWITCH, 1); // 1 = Normal
    gpio_put(OUT_TEMP_SWITCH, 1);

    while (true) {
        bool mda_cranking = gpio_get(IN_START_RELAY);
        bool mda_fuel_on  = gpio_get(IN_FUEL_RELAY);

        if (mda_cranking) {
            gpio_put(LED_CRANKING, 1);
        } else {
            gpio_put(LED_CRANKING, 0);
        }

        // If MDA gives fuel, the engine "catches" and runs
        if (mda_fuel_on) {
            gpio_put(LED_RUNNING, 1);
            gpio_put(OUT_L1_VOLTAGE, 1);
            gpio_put(OUT_CHARGE_ALT, 1);
            gpio_put(OUT_OIL_SWITCH, 1);
            gpio_put(OUT_TEMP_SWITCH, 1);
        } else {
            gpio_put(LED_RUNNING, 0);
            gpio_put(OUT_L1_VOLTAGE, 0);
            gpio_put(OUT_CHARGE_ALT, 0);
        }

        sleep_ms(20); // Non-blocking tick
    }
    return 0;
}