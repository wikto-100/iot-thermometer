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
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Wifi credentials
set(WIFI_SSID "" CACHE STRING "Wi-Fi network name")
set(WIFI_PASSWORD "" CACHE STRING "Wi-Fi password")

# Set the path for the FreeRTOS kernel
set(FREERTOS_KERNEL_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../external/FreeRTOS-Kernel
)

# Set the path for the NRF24L01 driver
set(NRF24L01_PATH
    ${CMAKE_CURRENT_LIST_DIR}/../external/nrf24l01
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

project(base_station C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(base_station
    base_station.cpp
    ${COMMON_PATH}/hooks.cpp
    ${COMMON_PATH}/nrf24l01/driver_nrf24l01_interface.c
    ${COMMON_PATH}/nrf24l01/driver_nrf24l01_temperature.c
    nrf24l01/temperature_receiver.c
    ${NRF24L01_PATH}/src/driver_nrf24l01.c
    network/base_station_network.c
)

pico_set_program_name(base_station "base_station")
pico_set_program_version(base_station "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(base_station 1)
pico_enable_stdio_usb(base_station 1)

pico_add_library(base_station_http_content NOFLAG)

pico_set_lwip_httpd_content(
    base_station_http_content
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/network/http/404.html
    ${CMAKE_CURRENT_LIST_DIR}/network/http/index.shtml
)


target_compile_definitions(base_station PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
)

# Add the standard include files to the build
target_include_directories(base_station PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/nrf24l01
        ${CMAKE_CURRENT_LIST_DIR}/network
        ${COMMON_PATH}
        ${COMMON_PATH}/nrf24l01
        ${NRF24L01_PATH}/src
        ${NRF24L01_PATH}/interface
        ${PICO_LWIP_CONTRIB_PATH}/apps/httpd
)

# Add the standard library to the build
target_link_libraries(base_station
    pico_stdlib
    hardware_spi
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_http
    base_station_http_content
)


pico_add_extra_outputs(base_station)

