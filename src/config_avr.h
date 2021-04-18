#pragma once
#ifdef AVR
#include "Arduino.h"
#include "SoftwareSerial.h"

// Software Serial for logging
#define RXD2 12
#define TXD2 13
#define LOG soft_serial
#define LOG_SETUP soft_serial.begin(115200)

#define SERIAL_SPEED 9600
#define SERIAL_TIMEOUT 500
#define MAX_FREQ 50000
using namespace logic_analyzer;	

namespace logic_analyzer {

SoftwareSerial soft_serial(RXD2, TXD2); // RX, TX

/**
 * @brief AVR specific implementation Logic for the abstract PinReaderAbstract
 * 
 */
class PinReader : public PinReaderAbstract {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask -  PORTD:pins 0 to 7 / PORTB: pins 8 to 13 
        virtual PinBitArray readAll() {
            uint16_t result = ((uint16_t)PORTB & B00111111) << 8 | PORTD;
            return result >> start_pin;
        }

    private:
        int start_pin;
};


} // namespace

#endif

