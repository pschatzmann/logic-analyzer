cmake_minimum_required(VERSION 3.19.2)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.12)
# PICO initialization
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

include(FetchContent)
FetchContent_Declare( arduino GIT_REPOSITORY https://github.com/pschatzmann/pico-arduino.git  GIT_TAG  main)
FetchContent_MakeAvailable(arduino)

project(logic-analyzer-pico)

add_definitions(-DPICO -DARDUINO=183)

add_executable(logic-analyzer logic-analyzer-pico.cpp)
target_include_directories(logic-analyzer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Pull in our arduino library
target_link_libraries(logic-analyzer arduino)
# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(logic-analyzer)
