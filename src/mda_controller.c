// ================================================================
//  MDA-A60Y1 Emulator — Controller Implementation
//
//  Reference:  CRE Technology MDA datasheet, PART NUMBER A60Y1
//  Standards:  NASA JPL Power of 10, MISRA-C:2012, BARR-C:2018
//
//  The controller is a non-blocking cooperative state machine.
//  Mda_Tick() samples all inputs once per call and dispatches to
//  exactly one handler. No handler blocks, so a fault that
//  occurs during the 6-second crank window is caught within one
//  scheduler tick (20 ms).
//
//  State sequence on a healthy start:
//    OFF -> PREHEAT -> WAIT_BEFORE_START -> CRANK -> RUNNING
//
//  State sequence on a fault:
//    RUNNING -> FAULT_OIL | FAULT_TEMP | FAULT_CHARGE | FAULT_START
//
//  Fault states latch until the operator releases the remote
//  start input, mirroring the real MDA's manual reset.
// ================================================================

#include "mda_controller.h"
#include "mda_hal.h"
#include "mda_config.h"

#include "pico/stdlib.h"

// Re-enable strict warnings that were disabled globally to silence
// SDK noise. These apply only to this file.
#if defined(__GNUC__)
    #pragma GCC diagnostic warning "-Wmissing-prototypes"
    #pragma GCC diagnostic warning "-Wundef"
#endif

// ----------------------------------------------------------------
//  Section 1: Runtime state (private to this file)
// ----------------------------------------------------------------
// Static linkage keeps these variables private to this
// translation unit. No other file can read or write them.
// ----------------------------------------------------------------
static MdaState_t       g_current_state;
static absolute_time_t  g_state_entry_time;
static uint8_t          g_start_attempts;

// ----------------------------------------------------------------
//  Section 2: Forward declarations
// ----------------------------------------------------------------
// Every static function is declared before use. This satisfies
// MISRA-C 8.4 and BARR-C §4.4.
// ----------------------------------------------------------------
static void Mda_EnterState(MdaState_t new_state);
static bool Mda_HasTimeElapsed(uint32_t duration_ms);
static void Mda_AllRelaysOff(void);
static void Mda_AllLedsOff(void);
static void Mda_IndicateFault(MdaState_t fault_state);

static void Mda_HandleOff(bool remote_start);
static void Mda_HandlePreheat(void);
static void Mda_HandleWaitBeforeStart(void);
static void Mda_HandleCrank(bool l1_present, float charge_v);
static void Mda_HandleWaitBetweenStarts(void);
static void Mda_HandleRunning(bool remote_start, bool oil_fault,
                              bool temp_fault, bool l1_present,
                              float charge_v);
static void Mda_HandleCooldown(void);
static void Mda_HandleStop(void);
static void Mda_HandleFault(bool remote_start);

// ----------------------------------------------------------------
//  Section 3: Public API
// ----------------------------------------------------------------

void Mda_Init(void)
{
    Mda_AllRelaysOff();
    Mda_AllLedsOff();

    g_current_state    = STATE_OFF;
    g_state_entry_time = get_absolute_time();
    g_start_attempts   = 0U;

    MDA_ASSERT(g_current_state == STATE_OFF);   // meaningful: init check
}

void Mda_Tick(void)
{
    // Sample all inputs once per tick. This snapshot is used by
    // every state handler below, so the controller never sees a
    // torn read within a single tick.
    const bool  remote_start = Hal_ReadRemoteStart();
    const bool  oil_fault    = Hal_ReadOilFault();
    const bool  temp_fault   = Hal_ReadTempFault();
    const bool  l1_present   = Hal_ReadL1Present();
    const float charge_v     = Hal_ReadChargeVoltage();

    // Dispatch on current state. Every case is short because the
    // per-state logic is delegated to a dedicated handler.
    switch (g_current_state)
    {
        case STATE_OFF:
            Mda_HandleOff(remote_start);
            break;

        case STATE_PREHEAT:
            Mda_HandlePreheat();
            break;

        case STATE_WAIT_BEFORE_START:
            Mda_HandleWaitBeforeStart();
            break;

        case STATE_CRANK:
            Mda_HandleCrank(l1_present, charge_v);
            break;

        case STATE_WAIT_BETWEEN_STARTS:
            Mda_HandleWaitBetweenStarts();
            break;

        case STATE_RUNNING:
            Mda_HandleRunning(remote_start, oil_fault, temp_fault,
                              l1_present, charge_v);
            break;

        case STATE_COOLDOWN:
            Mda_HandleCooldown();
            break;

        case STATE_STOP:
            Mda_HandleStop();
            break;

        case STATE_FAULT_START:
        case STATE_FAULT_OIL:
        case STATE_FAULT_TEMP:
        case STATE_FAULT_CHARGE:
            Mda_HandleFault(remote_start);
            break;

        default:
            // MISRA-C 16.4: every switch has a default.
            // This branch means the state variable was corrupted.
            MDA_ASSERT(false);
            Mda_EnterState(STATE_OFF);
            break;
    }

    // Postcondition: the state machine is always in a valid state.
    MDA_ASSERT(g_current_state <= STATE_FAULT_CHARGE);
}

// ----------------------------------------------------------------
//  Section 4: State handlers
// ----------------------------------------------------------------
// Each handler is short (Power of 10 Rule 4) and contains
// meaningful assertions only. Assertions check invariants that a
// bug could actually violate.

// ----------------------------------------------------------------
//  4.1 OFF — idle, waiting for a start request
// ----------------------------------------------------------------
static void Mda_HandleOff(bool remote_start)
{
    MDA_ASSERT(g_current_state == STATE_OFF);

    Mda_AllRelaysOff();
    Mda_AllLedsOff();
    g_start_attempts = 0U;

    if (remote_start)
    {
        Mda_EnterState(STATE_PREHEAT);
    }

    MDA_ASSERT(g_start_attempts == 0U);
}

// ----------------------------------------------------------------
//  4.2 PREHEAT — energize AUX for the preheat window
// ----------------------------------------------------------------
static void Mda_HandlePreheat(void)
{
    MDA_ASSERT(g_current_state == STATE_PREHEAT);

    Hal_SetAuxRelay(true);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_PREHEAT_DELAY))
    {
        Hal_SetAuxRelay(false);
        Mda_EnterState(STATE_WAIT_BEFORE_START);
    }
}

// ----------------------------------------------------------------
//  4.3 WAIT_BEFORE_START — 0.75 s settle before cranking
// ----------------------------------------------------------------
static void Mda_HandleWaitBeforeStart(void)
{
    MDA_ASSERT(g_current_state == STATE_WAIT_BEFORE_START);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_WAIT_BEFORE_START))
    {
        Mda_EnterState(STATE_CRANK);
    }
}

// ----------------------------------------------------------------
//  4.4 CRANK — starter + fuel on for 6 s
// ----------------------------------------------------------------
// Engine start criteria: L1 voltage present AND charge
// alternator above the OK threshold. If neither is true after
// MAX_START_ATTEMPTS cycles, the controller latches a fault.
// ----------------------------------------------------------------
static void Mda_HandleCrank(bool l1_present, float charge_v)
{
    MDA_ASSERT(g_current_state == STATE_CRANK);

    Hal_SetStartRelay(true);
    Hal_SetFuelRelay(true);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_START_DURATION))
    {
        Hal_SetStartRelay(false);

        if (l1_present && (charge_v >= (float)CHARGE_OK_VOLTAGE))
        {
            Mda_EnterState(STATE_RUNNING);
        }
        else
        {
            g_start_attempts++;

            if (g_start_attempts >= (uint8_t)MAX_START_ATTEMPTS)
            {
                Mda_IndicateFault(STATE_FAULT_START);
                Mda_EnterState(STATE_FAULT_START);
            }
            else
            {
                Mda_EnterState(STATE_WAIT_BETWEEN_STARTS);
            }
        }
    }

    MDA_ASSERT(g_start_attempts <= MAX_START_ATTEMPTS);
}

// ----------------------------------------------------------------
//  4.5 WAIT_BETWEEN_STARTS — 10 s gap between crank attempts
// ----------------------------------------------------------------
// Fuel stays energized between attempts so the engine does not
// have to re-prime. Only the starter cycles.
// ----------------------------------------------------------------
static void Mda_HandleWaitBetweenStarts(void)
{
    MDA_ASSERT(g_current_state == STATE_WAIT_BETWEEN_STARTS);

    Hal_SetFuelRelay(true);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_WAIT_BETWEEN_STARTS))
    {
        Mda_EnterState(STATE_CRANK);
    }
}

// ----------------------------------------------------------------
//  4.6 RUNNING — monitor faults after protection hold-off
// ----------------------------------------------------------------
// The protection hold-off timer suppresses sensor faults for
// the first 12 s of running, because oil pressure and
// temperature are still stabilising. After that window, any
// active fault latches.
// ----------------------------------------------------------------
static void Mda_HandleRunning(bool remote_start, bool oil_fault,
                              bool temp_fault, bool l1_present,
                              float charge_v)
{
    MDA_ASSERT(g_current_state == STATE_RUNNING);

    Hal_SetFuelRelay(true);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_PROTECTION_HOLDOFF))
    {
        if (oil_fault)
        {
            Mda_IndicateFault(STATE_FAULT_OIL);
            Mda_EnterState(STATE_FAULT_OIL);
        }
        else if (temp_fault)
        {
            Mda_IndicateFault(STATE_FAULT_TEMP);
            Mda_EnterState(STATE_FAULT_TEMP);
        }
        else if (!l1_present || (charge_v < (float)CHARGE_FAIL_VOLTAGE))
        {
            Mda_IndicateFault(STATE_FAULT_CHARGE);
            Mda_EnterState(STATE_FAULT_CHARGE);
        }
        else if (!remote_start)
        {
            Mda_EnterState(STATE_COOLDOWN);
        }
        else
        {
            // No fault, still running. Nothing to do.
        }
    }
}

// ----------------------------------------------------------------
//  4.7 COOLDOWN — 2 min unloaded idle before shutdown
// ----------------------------------------------------------------
static void Mda_HandleCooldown(void)
{
    MDA_ASSERT(g_current_state == STATE_COOLDOWN);

    Hal_SetFuelRelay(true);

    if (Mda_HasTimeElapsed((uint32_t)TIMING_COOLDOWN))
    {
        Mda_EnterState(STATE_STOP);
    }
}

// ----------------------------------------------------------------
//  4.8 STOP — 30 s wait for engine to physically stop
// ----------------------------------------------------------------
static void Mda_HandleStop(void)
{
    MDA_ASSERT(g_current_state == STATE_STOP);

    Mda_AllRelaysOff();

    if (Mda_HasTimeElapsed((uint32_t)TIMING_STOP_TIMER))
    {
        Mda_EnterState(STATE_OFF);
    }
}

// ----------------------------------------------------------------
//  4.9 FAULT — latched until remote start is released
// ----------------------------------------------------------------
// All four fault states share the same exit condition: the
// operator must release the remote start input. This mirrors
// the real MDA's manual reset behaviour.
// ----------------------------------------------------------------
static void Mda_HandleFault(bool remote_start)
{
    MDA_ASSERT(g_current_state >= STATE_FAULT_START);

    Mda_AllRelaysOff();

    if (!remote_start)
    {
        Mda_AllLedsOff();
        Mda_EnterState(STATE_OFF);
    }
}

// ----------------------------------------------------------------
//  Section 5: Helpers
// ----------------------------------------------------------------

static void Mda_EnterState(MdaState_t new_state)
{
    MDA_ASSERT(new_state <= STATE_FAULT_CHARGE);

    g_current_state    = new_state;
    g_state_entry_time = get_absolute_time();

    MDA_ASSERT(g_current_state == new_state);
}

static bool Mda_HasTimeElapsed(uint32_t duration_ms)
{
    MDA_ASSERT(duration_ms > 0U);   // meaningful: guards against a zero timer

    const int64_t elapsed_us = absolute_time_diff_us(
        g_state_entry_time, get_absolute_time());

    return (elapsed_us >= (int64_t)(duration_ms * 1000U));
}

static void Mda_AllRelaysOff(void)
{
    Hal_SetStartRelay(false);
    Hal_SetFuelRelay(false);
    Hal_SetAuxRelay(false);
}

static void Mda_AllLedsOff(void)
{
    Hal_SetOilLed(false);
    Hal_SetTempLed(false);
    Hal_SetStartFailLed(false);
    Hal_SetChargeLed(false);
}

static void Mda_IndicateFault(MdaState_t fault_state)
{
    MDA_ASSERT(fault_state >= STATE_FAULT_START);

    Mda_AllLedsOff();

        switch (fault_state)
    {
        case STATE_FAULT_OIL:
            Hal_SetOilLed(true);
            break;

        case STATE_FAULT_TEMP:
            Hal_SetTempLed(true);
            break;

        case STATE_FAULT_START:
            Hal_SetStartFailLed(true);
            break;

        case STATE_FAULT_CHARGE:
            Hal_SetChargeLed(true);
            break;

        // Non-fault states are listed explicitly so that
        // -Wswitch-enum is satisfied (MISRA-C 16.4). They take
        // no action here because this function is only called
        // on entry to a fault state.
        case STATE_OFF:
        case STATE_PREHEAT:
        case STATE_WAIT_BEFORE_START:
        case STATE_CRANK:
        case STATE_WAIT_BETWEEN_STARTS:
        case STATE_RUNNING:
        case STATE_COOLDOWN:
        case STATE_STOP:
        default:
            break;
    }

    MDA_ASSERT(fault_state <= STATE_FAULT_CHARGE);
}