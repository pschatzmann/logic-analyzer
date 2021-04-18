/**
 * @file logic_analyzer.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for a sigrok LogicAnalyzer for the Raspberry Pico using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */


#define PINS_TYPE uint8_t
#define LOG Serial1

#include "Arduino.h"
#include "network.h"
#include "logic_analyzer.h"
#include "pin_reader_pico.h"

LogicAnalyzerESP32 logicAnalyzer;
int pinStart=0;
int numberOfPins=4;
bool loggingActive = true;
int16_t maxCaptureSize=10000;

void setup() {
    Serial.begin(921600);
    Serial1.begin(115200);
    logicAnalyzer.begin(Serial, new LogicAnalyzerPicoImpl(numberOfPins), maxCaptureSize, pinStart, numberOfPins);
}

void loop() {
}