/**
 * @file logic_analyzer.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for the sigrok LogicAnalyzer for the ESP32 and AVR processors using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */

#define PINS_TYPE uint16_t  // Select based on the number of pins and start pin to be recorded
#include "Arduino.h"
#include "network.h"
#include "logic_analyzer.h"
#include "config_esp32.h"
#include "config_avr.h"

LogicAnalyzer logicAnalyzer;
int pinStart=4;
int numberOfPins=8;
int32_t maxCaptureSize=10000;

void setup() {
    LOG_SETUP;
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    logicAnalyzer.setCaptureFrequency(MAX_FREQ);
    logicAnalyzer.begin(Serial, new PinReader(pinStart), maxCaptureSize, pinStart, numberOfPins);
}

void loop() {
    logicAnalyzer.processCommand();
}
