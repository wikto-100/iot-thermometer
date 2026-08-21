# ==============================================================================
# SENSOR - BUILD CONFIGURATION (CMake)
# ==============================================================================
#
# This project is built using CMake, a tool that reads a file like this one
# (called "CMakeLists.txt") and turns it into the actual low-level build
# instructions (a Makefile, or in this project's case, a "Ninja" build
# file) that compile every .c file into machine code and link them all
# together into the final program. In other words: this file is the
# recipe that says "these are the source files, these are the libraries
# we need, here is how to put it all together" for the sensor program -
# the one that has the DS18B20 temperature sensor physically attached,
# reads it, and sends readings wirelessly over an nRF24L01+ radio.
#
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Which physical board we are building for: a plain Raspberry Pi Pico 2
# ("pico2"). All of its communication happens over the separate nRF24L01+ radio chip,
# wired up over SPI.
set(PICO_BOARD pico2 CACHE STRING "Board type")
# Set the path for the FreeRTOS kernel
set(FREERTOS_KERNEL_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../external/FreeRTOS-Kernel
)
# Set the path for the NRF24L01 driver
set(NRF24L01_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../external/nrf24l01
)
# Set the path for the DS18B20 driver
#
# The DS18B20 is the actual temperature sensor chip attached to this
# board - a separate, third-party ("vendored") driver library for it lives
# in ../external/ds18b20, the same way the nRF24L01+ radio driver does.
# This board's own ds18b20/driver_ds18b20_temperature.c is a thin wrapper
# around it, with the specific settings this project needs (see that file
# for details).
set(DS18B20_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../external/ds18b20
)
# Set the path for code shared between sensor and base_station
set(COMMON_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../common
)
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in the FreeRTOS kernel
include(
    "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake"
)

project(sensor C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
#
# The full list of source files compiled into the "sensor" program: this
# board's own application code, the radio driver, and the DS18B20 sensor
# driver.
add_executable(sensor
    sensor.cpp
    ${COMMON_PATH}/task_utils.c
    ${COMMON_PATH}/temperature_format.c
    ${COMMON_PATH}/hooks.cpp
    ${COMMON_PATH}/nrf24l01/driver_nrf24l01_interface.c
    ${COMMON_PATH}/nrf24l01/driver_nrf24l01_temperature.c
    nrf24l01/temperature_sender.c
    ${NRF24L01_PATH}/src/driver_nrf24l01.c
    ds18b20/driver_ds18b20_interface.c
    ${DS18B20_PATH}/src/driver_ds18b20.c
    ds18b20/driver_ds18b20_temperature.c
)

pico_set_program_name(sensor "sensor")
pico_set_program_version(sensor "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(sensor 1)
pico_enable_stdio_usb(sensor 1)

# Add the standard library to the build
#
# No networking libraries here (no lwIP, no CYW43 Wi-Fi integration) -
# this board does no networking of its own; all of its communication
# happens over the nRF24L01+ radio instead (built from source, above,
# rather than being a pre-built library).
target_link_libraries(sensor
    pico_stdlib
    hardware_spi
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

# Add the standard include files to the build
target_include_directories(sensor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/nrf24l01
    ${COMMON_PATH}
    ${COMMON_PATH}/nrf24l01
    ${NRF24L01_PATH}/src
    ${NRF24L01_PATH}/interface
    ${CMAKE_CURRENT_LIST_DIR}/ds18b20
    ${DS18B20_PATH}/src
    ${DS18B20_PATH}/interface
)

# Generates the additional output files alongside the main program: a
# .uf2 file (the format used to flash the Pico by drag-and-drop), a .hex
# file, a memory map, and so on.
pico_add_extra_outputs(sensor)
