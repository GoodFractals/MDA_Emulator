// ================================================================
//  MDA-A60Y1 Emulator — Shared Configuration
//
//  Reference:  CRE Technology MDA datasheet, PART NUMBER A60Y1
//  Target:     Raspberry Pi Pico 2 W (RP2350)
//  Standards:  NASA JPL Power of 10, MISRA-C:2012, BARR-C:2018
//
//  This header contains only compile-time constants: pin
//  assignments, timing values, and electrical thresholds. It
//  defines no functions and allocates no storage, so it is safe
//  to include from any translation unit.
//
//  This is the single source of truth for every configurable
//  value in the project. If the datasheet changes, or the
//  wiring changes, only this file needs to be edited.
// ================================================================

#ifndef MDA_CONFIG_H
#define MDA_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// ----------------------------------------------------------------
//  Section 1: Test mode
// ----------------------------------------------------------------
// The datasheet specifies real-world timings (10-second preheat,
// 6-second crank, 2-minute cooldown). Waiting through a full
// cycle takes nearly three minutes, which is impractical for
// debugging.
//
// When MDA_TEST_MODE is true, every timing is divided by
// MDA_TEST_SCALE_FACTOR. With a factor of 20, the full cycle
// completes in about 9 seconds. Set to false before flashing to
// real hardware for an interview demonstration.
// ----------------------------------------------------------------
#define MDA_TEST_MODE           (false)
#define MDA_TEST_SCALE_FACTOR   (20U)

// Both operands are compile-time constants, so the ternary is
// resolved by the compiler. Zero runtime cost.
#define MDA_SCALED_MS(x) \
    (MDA_TEST_MODE ? ((x) / MDA_TEST_SCALE_FACTOR) : (x))

// ----------------------------------------------------------------
//  Section 2: Assertion macro (Power of 10 Rule 5)
// ----------------------------------------------------------------
// When MDA_DEBUG is defined, assertions check invariants and
// halt the processor on failure. When undefined, the macro
// expands to nothing, so release binaries have zero overhead.
//
// Rule 5 states that assertion density should average to two
// per function. The key word is "density" — it is an average
// across the codebase, not a quota per function. And the rule
// explicitly prohibits filler assertions like assert(true),
// because a static analyzer can prove they never fail.
//
// The correct use of this macro is to check invariants that a
// bug could actually violate: state preconditions, postconditions,
// enum bounds, and counter ranges. If a check can never fail,
// it belongs in a comment, not an assertion.
// ----------------------------------------------------------------
#define MDA_DEBUG
#ifdef MDA_DEBUG
    #define MDA_ASSERT(expr)  assert(expr)
#else
    #define MDA_ASSERT(expr)  ((void)0)
#endif

// ----------------------------------------------------------------
//  Section 3: State machine enumeration
// ----------------------------------------------------------------
// The MDA is a finite state machine. At any instant, the
// controller is in exactly one of the states below. Transitions
// happen only inside the controller's tick function.
//
// The explicit "= 0" on the first value and the lack of gaps
// make the ordering of states part of the contract. Code that
// relies on "state >= STATE_FAULT_START" depends on this order.
// ----------------------------------------------------------------
typedef enum
{
    STATE_OFF                   = 0,   // Idle. All outputs off.
    STATE_PREHEAT               = 1,   // Energizing preheat output.
    STATE_WAIT_BEFORE_START     = 2,   // 0.75 s settle before crank.
    STATE_CRANK                 = 3,   // Starter + fuel energized.
    STATE_WAIT_BETWEEN_STARTS   = 4,   // 10 s gap between attempts.
    STATE_RUNNING               = 5,   // Engine running, monitoring.
    STATE_COOLDOWN              = 6,   // Load removed, idling down.
    STATE_STOP                  = 7,   // Fuel off, waiting for stop.
    STATE_FAULT_START           = 8,   // Latched: start failure.
    STATE_FAULT_OIL             = 9,   // Latched: low oil pressure.
    STATE_FAULT_TEMP            = 10,  // Latched: high temperature.
    STATE_FAULT_CHARGE          = 11   // Latched: charge failure.
} MdaState_t;

// ----------------------------------------------------------------
//  Section 4: Datasheet timing constants
// ----------------------------------------------------------------
// Every value in this block is traceable to a specific line in
// the CRE Technology datasheet. If a value seems arbitrary, it
// is not — it came from a document.
// ----------------------------------------------------------------
enum
{
    TIMING_PREHEAT_DELAY        = MDA_SCALED_MS(10000U),  // 10 s
    TIMING_WAIT_BEFORE_START    = MDA_SCALED_MS(750U),    // 0.75 s
    TIMING_START_DURATION       = MDA_SCALED_MS(6000U),   // 6 s
    TIMING_WAIT_BETWEEN_STARTS  = MDA_SCALED_MS(10000U),  // 10 s
    TIMING_PROTECTION_HOLDOFF   = MDA_SCALED_MS(12000U),  // 12 s
    TIMING_COOLDOWN             = MDA_SCALED_MS(120000U), // 2 min
    TIMING_STOP_TIMER           = MDA_SCALED_MS(30000U)   // 30 s
};

// ----------------------------------------------------------------
//  Section 5: Electrical constants
// ----------------------------------------------------------------
// Thresholds and conversion factors for the charge alternator
// ADC. The external voltage divider reduces the 0-30 VDC battery
// rail to the 0-3.3 V range of the RP2350 ADC.
//
// The RP2350 has a 12-bit ADC with a 3.3 V reference, so full
// scale is 4095 counts. The divider ratio of 11 corresponds to a
// 10 kΩ / 1 kΩ pair, which is the standard automotive voltage
// sense configuration.
// ----------------------------------------------------------------
enum
{
    ADC_SAMPLES             = 16U,   // oversampling count
    MAX_START_ATTEMPTS      = 3U,    // datasheet: 3 start cycles
    CHARGE_FAIL_VOLTAGE     = 6U,    // DC — fault threshold
    CHARGE_OK_VOLTAGE       = 12U    // DC — running threshold
};

#define ADC_VOLTAGE_DIVIDER     (11.0f)
#define ADC_VREF                (3.3f)
#define ADC_MAX_COUNTS          (4095.0f)

#endif // MDA_CONFIG_H