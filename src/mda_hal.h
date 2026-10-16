// ================================================================
//  MDA-A60Y1 Emulator — Hardware Abstraction Layer Interface
//
//  All hardware access flows through this interface. The
//  controller calls semantic operations ("set fuel relay") and
//  never touches a GPIO directly. This is what makes the
//  controller testable on a host PC with a mock HAL.
//
//  The interface exposes *intent*, not *implementation*. If the
//  project were ported to a different MCU, this header would stay
//  the same and only the .c file would change.
// ================================================================

#ifndef MDA_HAL_H
#define MDA_HAL_H

#include <stdbool.h>

// ----------------------------------------------------------------
//  Section 1: Lifecycle
// ----------------------------------------------------------------
// Must be called once at startup, before any other HAL function.
// Configures every GPIO direction, pull mode, and the ADC.
// ----------------------------------------------------------------
void Hal_Init(void);

// ----------------------------------------------------------------
//  Section 2: Input reads
// ----------------------------------------------------------------
// All digital reads apply the correct polarity internally, so
// the controller sees "true" when the condition is present.
// ----------------------------------------------------------------
bool  Hal_ReadRemoteStart(void);
bool  Hal_ReadOilFault(void);
bool  Hal_ReadTempFault(void);
bool  Hal_ReadL1Present(void);
float Hal_ReadChargeVoltage(void);

// ----------------------------------------------------------------
//  Section 3: Output writes — relays
// ----------------------------------------------------------------
// These drive the three relay outputs. The controller thinks in
// terms of the load, not the GPIO.
// ----------------------------------------------------------------
void Hal_SetStartRelay(bool state);
void Hal_SetFuelRelay(bool state);
void Hal_SetAuxRelay(bool state);

// ----------------------------------------------------------------
//  Section 4: Output writes — front-panel LEDs
// ----------------------------------------------------------------
// One function per indicator. The controller never touches a
// GPIO bank directly.
// ----------------------------------------------------------------
void Hal_SetOilLed(bool state);
void Hal_SetTempLed(bool state);
void Hal_SetStartFailLed(bool state);
void Hal_SetChargeLed(bool state);

#endif // MDA_HAL_H