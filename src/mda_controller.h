// ================================================================
//  MDA-A60Y1 Emulator — Controller Interface
//
//  The controller implements the MDA state machine. It reads
//  inputs and drives outputs exclusively through the HAL, which
//  means it can be unit-tested on a host PC with a mock HAL.
//
//  Two functions. That is the entire public surface.
// ================================================================

#ifndef MDA_CONTROLLER_H
#define MDA_CONTROLLER_H

// ----------------------------------------------------------------
//  Lifecycle
// ----------------------------------------------------------------
// Mda_Init() must be called once at startup, after Hal_Init().
// It puts the state machine into its safe initial condition.
//
// Mda_Tick() must be called once per scheduler period. It is
// fully non-blocking and safe to call from any cooperative loop.
// ----------------------------------------------------------------
void Mda_Init(void);
void Mda_Tick(void);

#endif // MDA_CONTROLLER_H