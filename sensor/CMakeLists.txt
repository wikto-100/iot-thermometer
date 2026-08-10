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

add_executable(sensor
    sensor.cpp
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

pico_add_extra_outputs(sensor)

