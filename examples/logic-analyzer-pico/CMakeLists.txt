cmake_minimum_required(VERSION 3.19.2)
# PICO initialization
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
set(PICO_SDK_ARDUINO_PATH $ENV{PICO_SDK_ARDUINO_PATH})
set(ARDUINO_SKETCH_NAME "logic_analyzer" )

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project("${ARDUINO_SKETCH_NAME}")


## Define all Source files 
set(ARDUINO_SKETCH_SOURCE  "logic_analyzer.cpp" )

include("${PICO_SDK_ARDUINO_PATH}/ArduinoSketch.cmake")

