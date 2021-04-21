/**
 * @file logic_analyzer.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for the sigrok LogicAnalyzer for the ESP32 and AVR processors using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */

#include "Arduino.h"
#include "logic_analyzer.h"

using namespace logic_analyzer;  

int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

LogicAnalyzer logicAnalyzer;
Capture capture;


void setup() {
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    logicAnalyzer.begin(Serial, capture, MAX_FREQ, MAX_FREQ_THRESHOLD, MAX_CAPTURE_SIZE, pinStart, numberOfPins);
}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}
