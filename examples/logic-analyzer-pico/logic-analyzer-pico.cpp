/**
 * @file logic_analyzer.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for a sigrok LogicAnalyzer for the Raspberry Pico using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */

#include "Arduino.h"
#include "logic_analyzer.h"

using namespace logic_analyzer;  

LogicAnalyzer logicAnalyzer;
int pinStart=0;
int numberOfPins=4;
int16_t maxCaptureSize=10000;
int32_t maxCaptureFreq=1000000;

void setup() {
    Serial.begin(921600);
    Serial1.begin(115200);
    logicAnalyzer.begin(Serial, new PinReader(numberOfPins),maxCaptureFreq, maxCaptureSize, pinStart, numberOfPins);
}

void loop() {
}