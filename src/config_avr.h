#pragma once
#ifdef AVR
#include "Arduino.h"
#include "SoftwareSerial.h"

#define MAX_CAPTURE_SIZE 500
#define SERIAL_SPEED 9600
#define SERIAL_TIMEOUT 500
#define MAX_FREQ 100000
#define MAX_FREQ_THRESHOLD 100000
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
// Arduino Mega: pins 22-29 are mapped in order to PORTA
#define START_PIN 22
#define DESCRIPTION "Arduino-AVR-Mega"
#else
#define START_PIN 0
#define DESCRIPTION "Arduino-AVR"
#endif
#define PIN_COUNT sizeof(PinBitArray)*8

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


#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)

/**
 * @brief Arduino Mega specific implementation Logic for the PinReader.
 * The Mega GPIO numbers are not mapped to PORTB/PORTD like on the Uno/Nano:
 * we read the port register that contains the start pin. Supported start pins
 * with a fast register read: 22-29 (PINA), A0-A7 (PINF) and A8-A15 (PINK).
 * For any other start pin we fall back to digitalRead().
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class PinReader  {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
          if (startPin >= 22 && startPin <= 29) {
            reg = &PINA;
            shift = startPin - 22;
          } else if (startPin >= A0 && startPin <= A7) {
            reg = &PINF;
            shift = startPin - A0;
          } else if (startPin >= A8 && startPin <= A15) {
            reg = &PINK;
            shift = startPin - A8;
          }
        }

        /// reads all pins and provides the result as bitmask
        inline PinBitArray readAll() {
            if (reg != nullptr) {
                return *reg >> shift;
            }
            // slow generic fallback
            PinBitArray result = 0;
            for (int j = 0; j < (int)sizeof(PinBitArray) * 8; j++) {
                if (digitalRead(start_pin + j)) result |= (1 << j);
            }
            return result;
        }

    private:
        int start_pin;
        volatile uint8_t *reg = nullptr;
        uint8_t shift = 0;
};

#else

/**
 * @brief AVR specific implementation Logic for the PinReader (Uno/Nano pin layout)
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class PinReader  {
    public:
        PinReader(int startPin){
          this->start_pin = startPin;
        }

        /// reads all pins and provides the result as bitmask -  PIND:pins 0 to 7 / PINB: pins 8 to 13
        inline PinBitArray readAll() {
            // Use the PINx input registers (actual physical pin state), not the
            // PORTx output-latch registers - PORTx does not reflect the real
            // voltage on pins configured as INPUT.
            uint16_t result = ((uint16_t)PINB & B00111111) << 8 | PIND;
            return result >> start_pin;
        }

    private:
        int start_pin;
};

#endif

} // namespace

#endif

