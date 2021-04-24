# -- CMAKE for Rasperry Pico
# -- author Phil Schatzmann
# -- copyright GPLv3

cmake_minimum_required(VERSION 3.12)
# PICO initialization
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Standard Sketch logic
project(logic_analyzer)
add_subdirectory(examples/logic-analyzer)
add_subdirectory(examples/logic-analyzer-test)
add_subdirectory(examples/logic-analyzer-pico)
add_subdirectory(examples/logic-analyzer-pico-pio)
