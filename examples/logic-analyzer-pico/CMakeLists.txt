# -- CMAKE for Rasperry Pico
# -- author Phil Schatzmann
# -- copyright GPLv3

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

# Install pico-arduino
include(FetchContent)
FetchContent_Declare( arduino GIT_REPOSITORY https://github.com/pschatzmann/pico-arduino.git  GIT_TAG  main)
FetchContent_MakeAvailable(arduino)

project(logic-analyzer-pico)
add_definitions(-DARDUINO_ARCH_RP2040 -DPICO -DARDUINO=183)

# Compile Arduino Sketch
set(ARDUINO_SKETCH ${CMAKE_CURRENT_BINARY_DIR}/logic-analyzer-pico.cpp)
file(GENERATE OUTPUT ${ARDUINO_SKETCH} INPUT ${CMAKE_CURRENT_SOURCE_DIR}/logic-analyzer-pico.ino )
add_executable(logic-analyzer-pico ${ARDUINO_SKETCH})
target_include_directories(logic-analyzer-pico PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Pull in our arduino library
target_link_libraries(logic-analyzer-pico arduino pico_stdlib hardware_uart)
# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(logic-analyzer-pico)