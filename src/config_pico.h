/**
 * @file logic_analyzer_generic.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Rasperry Pico processor specific implementation of SUMP based logic analyzer
 *  
 */

#pragma once

#ifdef ARDUINO_ARCH_RP2040
#include "Arduino.h"
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */

// processor specific settings
#define MAX_CAPTURE_SIZE 65535  
#define SERIAL_SPEED 921600
#define SERIAL_TIMEOUT 50
#define MAX_FREQ 2203225
#define MAX_FREQ_THRESHOLD 661400
#define START_PIN 6
#define PIN_COUNT sizeof(PinBitArray)*8
#define DESCRIPTION "Arduino-Pico"

namespace logic_analyzer {

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint8_t PinBitArray;

/**
 * @brief Pico specific implementation Logic for the PinReader
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PinReader  {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask
        inline PinBitArray readAll() {
            return gpio_get_all()>>start_pin;
        }

    private:
        int start_pin;
};

}

#endif