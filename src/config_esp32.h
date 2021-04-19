#pragma once
#ifdef ESP32
#include "Arduino.h"

// processor specific settings
#define MAX_CAPTURE_SIZE 10000
#define SERIAL_SPEED 921600
#define SERIAL_TIMEOUT 50
#define MAX_FREQ 500000

// led pin number is specific to your esp32 board
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 
#endif

// Use Serial1 for logging
#ifndef LOG
#define LOG Serial1
#define RXD2 16
#define TXD2 17
#define LOG_SETUP Serial1.begin(115200, SERIAL_8N1, RXD2, TXD2)
#endif

namespace logic_analyzer {

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint16_t PinBitArray;

/**
 * @brief ESP32 specific implementation Logic for the abstract PinReaderAbstract
 * 
 */
class PinReader {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask
        inline PinBitArray readAll() {
          uint32_t input = REG_READ(GPIO_IN_REG) >> start_pin;
          return input;
        }

    private:
        int start_pin;
};


} // namespace

#endif

