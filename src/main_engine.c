// ================================================================
//  Engine Simulator — Application Firmware
//
//  Target:     Raspberry Pi Pico W (RP2040)
//  Purpose:    Reads MDA relay outputs and simulates engine sensors.
// ================================================================

// Engine Simulator — Application Firmware
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

// Fault Injection Buttons
#define BTN_OIL_FAULT  18 
#define BTN_TEMP_FAULT 16

int main(void) {
    stdio_init_all();

    // MDA Inputs
    gpio_init(IN_START_RELAY); gpio_set_dir(IN_START_RELAY, GPIO_IN); gpio_pull_down(IN_START_RELAY);
    gpio_init(IN_FUEL_RELAY);  gpio_set_dir(IN_FUEL_RELAY, GPIO_IN);  gpio_pull_down(IN_FUEL_RELAY);
    gpio_init(IN_AUX_RELAY);   gpio_set_dir(IN_AUX_RELAY, GPIO_IN);   gpio_pull_down(IN_AUX_RELAY);

    // Sensor Outputs
    gpio_init(OUT_L1_VOLTAGE);  gpio_set_dir(OUT_L1_VOLTAGE, GPIO_OUT);
    gpio_init(OUT_CHARGE_ALT);  gpio_set_dir(OUT_CHARGE_ALT, GPIO_OUT);
    gpio_init(OUT_TEMP_SWITCH); gpio_set_dir(OUT_TEMP_SWITCH, GPIO_OUT);

    // Buttons
    gpio_init(BTN_OIL_FAULT);   gpio_set_dir(BTN_OIL_FAULT, GPIO_IN);  gpio_pull_up(BTN_OIL_FAULT);
    gpio_init(BTN_TEMP_FAULT);  gpio_set_dir(BTN_TEMP_FAULT, GPIO_IN); gpio_pull_up(BTN_TEMP_FAULT);

    // LEDs
    gpio_init(LED_CRANKING); gpio_set_dir(LED_CRANKING, GPIO_OUT);
    gpio_init(LED_RUNNING);  gpio_set_dir(LED_RUNNING, GPIO_OUT);
    gpio_init(LED_FAULT);    gpio_set_dir(LED_FAULT, GPIO_OUT);

    // Initial state
    gpio_put(OUT_L1_VOLTAGE, 0);
    gpio_put(OUT_CHARGE_ALT, 0);
    gpio_put(OUT_TEMP_SWITCH, 1); 

    while (true) {
        bool mda_cranking = gpio_get(IN_START_RELAY);
        bool mda_fuel_on  = gpio_get(IN_FUEL_RELAY);
        
        bool oil_fault_btn = (gpio_get(BTN_OIL_FAULT) == 0); 
        bool temp_fault_btn = (gpio_get(BTN_TEMP_FAULT) == 0); 

        // Handle LEDs
        gpio_put(LED_CRANKING, mda_cranking);
        gpio_put(LED_FAULT, (oil_fault_btn || temp_fault_btn) ? 1 : 0);

        if (mda_fuel_on) {
            gpio_put(LED_RUNNING, 1);
            gpio_put(OUT_L1_VOLTAGE, 1);
            gpio_put(OUT_CHARGE_ALT, 1);

            // Drive Temp Fault line based on button
            if (temp_fault_btn) {
                gpio_put(OUT_TEMP_SWITCH, 0); // Pull LOW to signal fault
            } else {
                gpio_put(OUT_TEMP_SWITCH, 1); // HIGH = Normal
            }
        } else {
            gpio_put(LED_RUNNING, 0);
            gpio_put(OUT_L1_VOLTAGE, 0);
            gpio_put(OUT_CHARGE_ALT, 0);
            gpio_put(OUT_TEMP_SWITCH, 1); 
        }

        sleep_ms(20);
    }
    return 0;
}