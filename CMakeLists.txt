# Force the correct board for the Pico 2 W. Must precede the
# extension's pico-vscode.cmake include, which otherwise caches
# PICO_BOARD as 'pico' (the original RP2040 board).
set(PICO_BOARD    "pico2_w" CACHE STRING "Pico board variant" FORCE)
set(PICO_PLATFORM "rp2350"  CACHE STRING "Pico platform"     FORCE)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# ================================================================
#  MDA-A60Y1 Emulator — CMake Build Configuration
#
#  Reference:  CRE Technology MDA datasheet, PART NUMBER A60Y1
#  Target:     Raspberry Pi Pico 2 W (RP2350)
#  Standards:  MISRA-C:2012, BARR-C:2018, NASA JPL Power of 10
#
#  Design intent:
#    - Deterministic build for predictable timing.
#    - Aggressive dead-code elimination to minimize flash usage.
#    - Strict compiler warnings to surface MISRA-C violations.
# ================================================================

cmake_minimum_required(VERSION 3.17)

# Local SDK import (created by the Pico VS Code extension).
include(pico_sdk_import.cmake)

project(MDA_Emulator LANGUAGES C CXX ASM)

# Build type — smaller binary is preferred over raw speed.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MISRA-C / BARR-C warning flags.
set(MDA_WARNING_FLAGS
    -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
    -Wcast-align -Wstrict-prototypes 
    -Wswitch-default -Wswitch-enum
)

set(MDA_SIZE_FLAGS  -ffunction-sections -fdata-sections)
set(MDA_LINK_FLAGS -Wl,--gc-sections)

pico_sdk_init()

# Application library — three source files, one include dir.
add_library(mda_core STATIC
    src/main.c
    src/mda_hal.c
    src/mda_controller.c
)

target_include_directories(mda_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(mda_core PRIVATE
    ${MDA_WARNING_FLAGS}
    ${MDA_SIZE_FLAGS}
)

target_link_libraries(mda_core PUBLIC
    pico_stdlib
    hardware_adc
    pico_cyw43_arch_none
)

# --- MDA Executable ---
add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE mda_core)
target_link_options(${PROJECT_NAME} PRIVATE ${MDA_LINK_FLAGS} -Wl,-Map=$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.map)

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

# --- ENGINE SIMULATOR EXECUTABLE ---
add_executable(Engine_Simulator src/main_engine.c)

target_compile_options(Engine_Simulator PRIVATE
    ${MDA_WARNING_FLAGS}
    ${MDA_SIZE_FLAGS}
)

target_link_libraries(Engine_Simulator PRIVATE
    pico_stdlib
    pico_cyw43_arch_none
)

target_link_options(Engine_Simulator PRIVATE ${MDA_LINK_FLAGS} -Wl,-Map=$<TARGET_FILE_BASE_NAME:Engine_Simulator>.map)

pico_enable_stdio_uart(Engine_Simulator 1)
pico_enable_stdio_usb(Engine_Simulator 0)
pico_add_extra_outputs(Engine_Simulator)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)