/**
 * @file logic_analyzer_test.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Tests different capturing frequencies and compares them with the real measured frequency
 */

#define PINS_TYPE uint8_t  // Select based on the number of pins and start pin to be recorded

#include "Arduino.h"
#include "network.h"
#include "logic_analyzer.h"
#include "pin_reader_esp32.h"
#include "pin_reader_avr.h"
#undef LOG

LogicAnalyzer logicAnalyzer;
int pinStart=4;
int numberOfPins=8;
int32_t maxCaptureSize=1000;

uint32_t frequencies[] = { 1000, 10000, 100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000 };

void setup() {
    Serial.begin(115200);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    Serial.println("setup");
    logicAnalyzer.begin(Serial, new PinReader(pinStart), maxCaptureSize, pinStart, numberOfPins);

    for(auto &f : frequencies){
        testFrequency(f);
    }
}

void testFrequency(uint32_t frq){
    Serial.print("testing ");
    Serial.print(frq);
    logicAnalyzer.reset();
    logicAnalyzer.setCaptureFrequency(frq);
    uint64_t start = millis();
    logicAnalyzer.capture(false); // captures maxCaptureSize samples w/o dump
    uint64_t end = millis();

    uint32_t measured_freq = maxCaptureSize * 1000 / (end - start);

    Serial.print(" -> ");
    Serial.println(measured_freq);

}


void loop() {
    
}
