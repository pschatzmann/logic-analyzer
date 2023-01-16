#pragma once
#ifdef ESP8266
#include "Arduino.h"
#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>

// processor specific settings
#define MAX_CAPTURE_SIZE 50000
#define SERIAL_SPEED 921600
#define SERIAL_TIMEOUT 50
#define MAX_FREQ 1038680
#define MAX_FREQ_THRESHOLD 533200
#define START_PIN 12
#define PIN_COUNT 4
#define DESCRIPTION "Arduino-ESP8266"


namespace logic_analyzer {

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint8_t PinBitArray;


/**
 * @brief ESP8266 specific implementation Logic for PinReader
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class PinReader  {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask
        inline PinBitArray readAll() {
          uint32_t input = (GPIO_REG_READ(GPIO_IN_ADDRESS) & (uint32)(0b1111111111111111)) >> start_pin;
          return input;
        }

    private:
        int start_pin;
};


} // namespace

#endif

