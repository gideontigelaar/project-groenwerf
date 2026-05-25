cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(grass_monitor_pico C CXX ASM)

pico_sdk_init()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/credentials.h")
    message(FATAL_ERROR "Missing credentials.h — copy include/credentials.h.template and fill in details.")
endif()

add_executable(grass_monitor_pico
    src/main.cpp
    src/sensors/tof_sensor.cpp
    src/sensors/adxl345.cpp
    src/sensors/rcwl1604.cpp
    src/sensors/vma430_gps.cpp
    src/sensors/tmp36.cpp
    src/processing/sensor_processor.cpp
    src/processing/median_filter.cpp
    src/processing/vibration_processor.cpp
    src/networkmanager.cpp
)

target_include_directories(grass_monitor_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grass_monitor_pico
    pico_stdlib
    hardware_i2c
    hardware_uart
    hardware_adc
    pico_cyw43_arch_lwip_threadsafe_background
)

pico_enable_stdio_uart(grass_monitor_pico 0)
pico_enable_stdio_usb(grass_monitor_pico 1)
pico_add_extra_outputs(grass_monitor_pico)