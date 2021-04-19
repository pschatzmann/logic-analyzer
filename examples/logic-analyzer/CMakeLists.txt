# -- CMAKE for Rasperry Pico
# -- author Phil Schatzmann
# -- copyright GPLv3

cmake_minimum_required(VERSION 3.19.2)
# PICO initialization
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(logic-analyzer-pico CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Install pico-arduino
include(FetchContent)
FetchContent_Declare(arduino GIT_REPOSITORY https://github.com/pschatzmann/pico-arduino.git  GIT_TAG  main)
FetchContent_MakeAvailable(arduino)

add_definitions(-DPICO -DARDUINO=183 )

# Compile Arduino Sketch
#set(ARDUINO_SKETCH logic-analyzer.ino)
#add_definitions("-x c++")
#set_source_files_properties(${ARDUINO_SKETCH} PROPERTIES LANGUAGE CXX)
add_executable(logic-analyzer ${ARDUINO_SKETCH})
target_include_directories(logic-analyzer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Pull in our arduino library
target_link_libraries(logic-analyzer arduino)
# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(logic-analyzer)
