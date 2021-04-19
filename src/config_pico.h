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

// processor specific settings
#define MAX_CAPTURE_SIZE 10000
#define SERIAL_SPEED 921600
#define SERIAL_TIMEOUT 50
#define MAX_FREQ 500000
#define MAX_FREQ_THRESHOLD 500000
#define START_PIN 4
#define PIN_COUNT sizeof(PinBitArray)*8

// Use Serial1 for logging
#ifndef LOG
#define LOG Serial1
#define RXD2 16
#define TXD2 17
#endif


namespace logic_analyzer {

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint16_t PinBitArray;

void setupLogger() {
    LOG.begin(115200, SERIAL_8N1, RXD2, TXD2);
}


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