#pragma once
#ifdef ESP32
#include "Arduino.h"

#define PINS_TYPE uint16_t  // Select based on the number of pins and start pin to be recorded
#define MAX_CAPTURE_SIZE 10000

// pin number is specific to your esp32 board
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 
#endif

// User Serial1 for logging
#ifndef LOG
#define RXD2 16
#define TXD2 17
#define LOG Serial1
#define LOG_SETUP LOG.begin(115200, SERIAL_8N1, RXD2, TXD2)
#endif

#define SERIAL_SPEED 921600
#define SERIAL_TIMEOUT 50
#define MAX_FREQ 500000


using namespace logic_analyzer;	

namespace logic_analyzer {

/**
 * @brief ESP32 specific implementation Logic for the abstract PinReaderAbstract
 * 
 */
class PinReader : public PinReaderAbstract {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask
        virtual PinBitArray readAll() {
          uint32_t input = REG_READ(GPIO_IN_REG) >> start_pin;
          return input;
        }

    private:
        int start_pin;
};


} // namespace

#endif

