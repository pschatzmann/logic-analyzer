#pragma once
#ifdef AVR
#include "Arduino.h"
#include "SoftwareSerial.h"

#define MAX_CAPTURE_SIZE 500
#define SERIAL_SPEED 9600
#define SERIAL_TIMEOUT 500
#define MAX_FREQ 100000
#define MAX_FREQ_THRESHOLD 100000
#define START_PIN 0
#define PIN_COUNT sizeof(PinBitArray)*8
#define DESCRIPTION "Arduino-AVR"

// Software Serial for logging
#define LOG soft_serial
#define RXD2 12
#define TXD2 13


namespace logic_analyzer {

/// Define output
inline SoftwareSerial soft_serial(RXD2, TXD2); // RX, TX

void setupLogger() {
    LOG.begin(115200);
}

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef uint8_t PinBitArray;


/**
 * @brief AVR specific implementation Logic for the PinReader
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class PinReader  {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask -  PORTD:pins 0 to 7 / PORTB: pins 8 to 13 
        inline PinBitArray readAll() {
            uint16_t result = ((uint16_t)PORTB & B00111111) << 8 | PORTD;
            return result >> start_pin;
        }

    private:
        int start_pin;
};


} // namespace

#endif

