// ================================================================
//  MDA-A60Y1 Emulator — Hardware Abstraction Layer
//
//  This file is the only one that knows which GPIO is connected
//  to what. If the physical wiring changes, only this file
//  changes. Nothing else in the project touches a GPIO number.
//
//  Design intent:
//    - Pin map is private (static enum, not in any header).
//    - All polarity handling is centralized here.
//    - All ADC scaling is centralized here.
//    - The controller receives semantic values, never raw counts.
// ================================================================

#include "mda_hal.h"
#include "mda_config.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// Re-enable strict warnings that were disabled globally to silence
// SDK noise. These apply only to this file.
#if defined(__GNUC__)
    #pragma GCC diagnostic warning "-Wmissing-prototypes"
    #pragma GCC diagnostic warning "-Wundef"
#endif

// ----------------------------------------------------------------
//  Section 1: Pin map (private to this file)
// ----------------------------------------------------------------
// Every GPIO used by the MDA is named here. If the physical
// wiring changes, only this block changes.
// ----------------------------------------------------------------
typedef enum
{
    // Digital inputs (negative-closing per datasheet).
    PIN_REMOTE_START    = 16,
    PIN_OIL_SWITCH      = 18,
    PIN_TEMP_SWITCH     = 19,

    // Digital input (positive-closing).
    PIN_L1_PRESENT      = 17,

    // Analog input.
    PIN_CHARGE_ALT      = 26,

    // Relay driver outputs.
    PIN_START_RELAY     = 10,
    PIN_FUEL_RELAY      = 11,
    PIN_AUX_RELAY       = 12,

    // Front-panel status LEDs.
    PIN_LED_OIL         = 7,
    PIN_LED_TEMP        = 8,
    PIN_LED_STARTFAIL   = 9,
    PIN_LED_CHARGE      = 13
} HalPin_t;

// ----------------------------------------------------------------
//  Section 2: Lifecycle
// ----------------------------------------------------------------

void Hal_Init(void)
{
    stdio_init_all();

    // Negative-closing switches use internal pull-ups so the pin
    // reads HIGH when the switch is open and LOW when closed.
    gpio_init((uint)PIN_REMOTE_START);
    gpio_set_dir((uint)PIN_REMOTE_START, GPIO_IN);
    gpio_pull_up((uint)PIN_REMOTE_START);

    gpio_init((uint)PIN_OIL_SWITCH);
    gpio_set_dir((uint)PIN_OIL_SWITCH, GPIO_IN);
    gpio_pull_up((uint)PIN_OIL_SWITCH);

    gpio_init((uint)PIN_TEMP_SWITCH);
    gpio_set_dir((uint)PIN_TEMP_SWITCH, GPIO_IN);
    gpio_pull_up((uint)PIN_TEMP_SWITCH);

    // L1 voltage presence is active-high: pull-down so that an
    // unconnected pin reads LOW (no voltage present).
    gpio_init((uint)PIN_L1_PRESENT);
    gpio_set_dir((uint)PIN_L1_PRESENT, GPIO_IN);
    gpio_pull_down((uint)PIN_L1_PRESENT);

    // ADC for charge alternator.
    adc_init();
    adc_gpio_init((uint)PIN_CHARGE_ALT);
    adc_select_input(0U);   // GPIO26 maps to ADC channel 0.

    // Relay outputs — all start LOW (de-energized).
    gpio_init((uint)PIN_START_RELAY);
    gpio_set_dir((uint)PIN_START_RELAY, GPIO_OUT);
    gpio_put((uint)PIN_START_RELAY, 0U);

    gpio_init((uint)PIN_FUEL_RELAY);
    gpio_set_dir((uint)PIN_FUEL_RELAY, GPIO_OUT);
    gpio_put((uint)PIN_FUEL_RELAY, 0U);

    gpio_init((uint)PIN_AUX_RELAY);
    gpio_set_dir((uint)PIN_AUX_RELAY, GPIO_OUT);
    gpio_put((uint)PIN_AUX_RELAY, 0U);

    // Status LEDs — all start off.
    gpio_init((uint)PIN_LED_OIL);
    gpio_set_dir((uint)PIN_LED_OIL, GPIO_OUT);
    gpio_put((uint)PIN_LED_OIL, 0U);

    gpio_init((uint)PIN_LED_TEMP);
    gpio_set_dir((uint)PIN_LED_TEMP, GPIO_OUT);
    gpio_put((uint)PIN_LED_TEMP, 0U);

    gpio_init((uint)PIN_LED_STARTFAIL);
    gpio_set_dir((uint)PIN_LED_STARTFAIL, GPIO_OUT);
    gpio_put((uint)PIN_LED_STARTFAIL, 0U);

    gpio_init((uint)PIN_LED_CHARGE);
    gpio_set_dir((uint)PIN_LED_CHARGE, GPIO_OUT);
    gpio_put((uint)PIN_LED_CHARGE, 0U);
}

// ----------------------------------------------------------------
//  Section 3: Input reads
// ----------------------------------------------------------------
// No assertions here. Every pin number is a compile-time
// constant, so bounds checks would be provably useless and
// therefore prohibited by Power of 10 Rule 5.
// ----------------------------------------------------------------

bool Hal_ReadRemoteStart(void)
{
    return (gpio_get((uint)PIN_REMOTE_START) == 0U);
}

bool Hal_ReadOilFault(void)
{
    return (gpio_get((uint)PIN_OIL_SWITCH) == 0U);
}

bool Hal_ReadTempFault(void)
{
    return (gpio_get((uint)PIN_TEMP_SWITCH) == 0U);
}

bool Hal_ReadL1Present(void)
{
    return (gpio_get((uint)PIN_L1_PRESENT) != 0U);
}

// ----------------------------------------------------------------
//  Section 4: ADC reader
// ----------------------------------------------------------------
// Reads the charge alternator voltage through the external
// divider. Sixteen samples are averaged to reject electrical
// noise from the simulated engine environment.
//
// Power of 10 Rule 2: the for loop has a fixed upper bound.
// ----------------------------------------------------------------
float Hal_ReadChargeVoltage(void)
{
    MDA_ASSERT(ADC_SAMPLES > 0U);   // meaningful: guards divide-by-zero

    uint32_t accumulator = 0U;

    for (uint8_t i = 0U; i < (uint8_t)ADC_SAMPLES; i++)
    {
        accumulator += (uint32_t)adc_read();
    }

    const float average_counts  = (float)accumulator / (float)ADC_SAMPLES;
    const float pin_voltage     = (average_counts * ADC_VREF) / ADC_MAX_COUNTS;
    const float battery_voltage = pin_voltage * ADC_VOLTAGE_DIVIDER;

    return battery_voltage;
}

// ----------------------------------------------------------------
//  Section 5: Output writes — relays
// ----------------------------------------------------------------

void Hal_SetStartRelay(bool state)
{
    gpio_put((uint)PIN_START_RELAY, state ? 1U : 0U);
}

void Hal_SetFuelRelay(bool state)
{
    gpio_put((uint)PIN_FUEL_RELAY, state ? 1U : 0U);
}

void Hal_SetAuxRelay(bool state)
{
    gpio_put((uint)PIN_AUX_RELAY, state ? 1U : 0U);
}

// ----------------------------------------------------------------
//  Section 6: Output writes — LEDs
// ----------------------------------------------------------------

void Hal_SetOilLed(bool state)
{
    gpio_put((uint)PIN_LED_OIL, state ? 1U : 0U);
}

void Hal_SetTempLed(bool state)
{
    gpio_put((uint)PIN_LED_TEMP, state ? 1U : 0U);
}

void Hal_SetStartFailLed(bool state)
{
    gpio_put((uint)PIN_LED_STARTFAIL, state ? 1U : 0U);
}

void Hal_SetChargeLed(bool state)
{
    gpio_put((uint)PIN_LED_CHARGE, state ? 1U : 0U);
}