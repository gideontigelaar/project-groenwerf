cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(grass_monitor_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(grass_monitor_pico
    src/main.cpp
    src/sensors/vl53l1x.cpp
    src/sensors/adxl345.cpp
    src/sensors/rcwl1604.cpp
    src/processing/sensor_processor.cpp
    src/processing/moving_average.cpp
    src/processing/median_filter.cpp
    src/processing/vibration_processor.cpp
)

target_include_directories(grass_monitor_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grass_monitor_pico
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_uart(grass_monitor_pico 1)
pico_enable_stdio_usb(grass_monitor_pico 1)
pico_add_extra_outputs(grass_monitor_pico)