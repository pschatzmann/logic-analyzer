/**
 * @file logic_analyzer_test.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Tests different capturing frequencies and compares them with the real measured frequency
 */

#include "Arduino.h"
#include "logic_analyzer.h"

using namespace logic_analyzer;  

LogicAnalyzer<PinBitArray> logicAnalyzer;
int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

uint32_t frequencies[] = { 50000, 100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000 };

void printLine() {
    Serial.println("--------------------------");
}

void testBufferSize() {
    Serial.print("buffer size: ");
    Serial.println(logicAnalyzer.size());
    printLine();
}

void testSingleSample() {
    Serial.print("Caputre Single Sample: ");
    logicAnalyzer.reset();
    logicAnalyzer.setStatus(TRIGGERED);
    logicAnalyzer.captureSampleFast();
    Serial.println(logicAnalyzer.available());
    printLine();
  
}

void testFrequency(uint32_t frq){
    Serial.print("testing ");
    Serial.print(frq);
    logicAnalyzer.reset();
    logicAnalyzer.setCaptureFrequency(frq);
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t start = micros();
    logicAnalyzer.captureAll(); // captures maxCaptureSize samples w/o dump
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);
    
    Serial.print(" -> ");
    Serial.print(measured_freq);
    Serial.print(" using delay microsec ");
    Serial.println((uint32_t)logicAnalyzer.delayTimeUs());
}

void testFrequencyMaxSpeed(){
    Serial.println("testing max speed ");
    logicAnalyzer.reset();
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t start = micros();
    logicAnalyzer.captureAllMaxSpeed();
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);

    Serial.print("time us:");
    Serial.println((double) (end - start));
    Serial.print("sample count:");
    Serial.println(logicAnalyzer.available());

    Serial.print("max speed: ");
    Serial.println(measured_freq);
    printLine();
}

void testPins() {
    logicAnalyzer.reset();
    for (int pin=pinStart;pin<pinStart+numberOfPins;pin++){
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        PinBitArray result = logicAnalyzer.captureSample();
        digitalWrite(pin, LOW);

        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(" -> ");
        Serial.print(result, BIN);
        Serial.println();
    }
    printLine();
}

void setup() {
    Serial.begin(115200);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    Serial.println("setup");
    logicAnalyzer.begin(Serial, new PinReader(pinStart), MAX_FREQ, MAX_FREQ_THRESHOLD,  MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    printLine();
    testSingleSample();
    testBufferSize();
    testFrequencyMaxSpeed();

    for(auto &f : frequencies){
        testFrequency(f);
    }
    printLine();

    testPins();
}

void loop() {
}