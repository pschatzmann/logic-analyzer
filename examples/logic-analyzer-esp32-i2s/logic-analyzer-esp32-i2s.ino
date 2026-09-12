/**
 * @file logic-analyzer-esp32-i2s.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Hardware-timed (I2S0 + DMA) SUMP logic analyzer for the original ESP32.
 *
 * This uses the I2S0 peripheral's "camera" mode to sample an 8 bit parallel
 * GPIO bus with no CPU involvement per sample, instead of the bit-banged
 * software loop used by the plain logic-analyzer-esp32 example.
 *
 * IMPORTANT WIRING: the ESP32 I2S0 peripheral can only sample on an
 * *external* clock in this mode. This sketch generates that clock itself on
 * GPIO32 (LEDC PWM) - you MUST connect a jumper wire from GPIO32 to GPIO34
 * (the clock input) for this to work at all.
 *
 * Data pins (8 bits): GPIO19..GPIO26.
 *
 * Not supported yet: triggering (always captures readCount samples starting
 * immediately on arm - same limitation as the Pico PIO example).
 */
#include "Arduino.h"

#ifndef CONFIG_IDF_TARGET_ESP32
#error "This sketch only works on the original ESP32 (not S2/S3/C3)"
#endif

#include "capture_esp32_i2s.h"

using namespace logic_analyzer;

LogicAnalyzer logicAnalyzer;
CaptureESP32I2S capture(/*pinStart*/ 19, /*clkOutPin*/ 32, /*clkInPin*/ 34);

void setup() {
    Serial.begin(SERIAL_SPEED);
    Serial.setTimeout(SERIAL_TIMEOUT);

    pinMode(LED_BUILTIN, OUTPUT);
    logicAnalyzer.setDescription("Arduino-ESP32-I2S");
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, 19, 8);
}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}
