cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(vl53l1x_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(vl53l1x_pico
    main.cpp
    vl53l1x.cpp
)

target_include_directories(vl53l1x_pico PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vl53l1x_pico
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_uart(vl53l1x_pico 1)
pico_enable_stdio_usb(vl53l1x_pico 1)
pico_add_extra_outputs(vl53l1x_pico)