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

using namespace logic_analyzer;

namespace logic_analyzer {

/**
 * @brief Pico specific implementation Logic for the abstract PinReaderAbstract
 * 
 */
class PinReader : public PinReaderAbstract {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask
        virtual PinBitArray readAll() {
            return gpio_get_all()>>start_pin;
        }

    private:
        int start_pin;
};


}

#endif