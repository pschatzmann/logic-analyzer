/**
 * @file logic_analyzer_generic.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Rasperry Pico processor specific implementation of SUMP based logic analyzer
 *  
 */

#pragma once

#ifdef PICO
#include "Arduino.h"
#define LOG Serial1

namespace logic_analyzer {

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint16_t PinBitArray;

/**
 * @brief Pico specific implementation Logic for the abstract PinReaderAbstract
 * 
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