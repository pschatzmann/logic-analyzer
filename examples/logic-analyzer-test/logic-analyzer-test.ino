/**
 * @file logic_analyzer_test.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Tests different capturing frequencies and compares them with the real measured frequency
 */

#include "Arduino.h"
#define LOG Serial
#include "logic_analyzer.h"
#include "capture_raspberry_pico.h"

using namespace logic_analyzer;  

int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;
LogicAnalyzer logicAnalyzer;
Capture capture(MAX_FREQ, MAX_FREQ_THRESHOLD);

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
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    capture.captureSampleFast();
    Serial.println(logicAnalyzer.available());
    printLine();
  
}

void testFrequency(uint32_t frq){
    Serial.print("testing ");
    Serial.print(frq);
    logicAnalyzer.clear();
    logicAnalyzer.setCaptureFrequency(frq);
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t start = micros();
    capture.captureAll(); // captures maxCaptureSize samples w/o dump
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);
    
    Serial.print(" -> ");
    Serial.print(measured_freq);
    Serial.print(" using delay microsec ");
    Serial.println((uint32_t)logicAnalyzer.delayTimeUs());
}

void testFrequencyMaxSpeed(){
    Serial.println("testing max speed ");
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t start = micros();
    capture.captureAllMaxSpeed();
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
    logicAnalyzer.clear();
    for (int pin=pinStart;pin<pinStart+numberOfPins;pin++){
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        PinBitArray result = capture.captureSample();
        digitalWrite(pin, LOW);

        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(" -> ");
        Serial.print(result, BIN);
        Serial.println();
    }
    printLine();
}

void testPIO() {
#ifdef ARDUINO_ARCH_RP2040
    PicoCapturePIO picoCapture(3759034);
    logicAnalyzer.begin(Serial, &picoCapture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    Serial.println("testing PIO");
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t run_time_us = picoCapture.testCapture(1.0);

    float measured_freq = 1000000.0 * MAX_CAPTURE_SIZE  / run_time_us;

    Serial.print("time us:");
    Serial.println((double) (run_time_us));

    Serial.print("max speed: ");
    Serial.println(measured_freq);
    printLine();    
#endif    
}

void setup() {
    Serial.begin(115200);  
    // wait for Serial to be ready
    while(!Serial);
    Serial.setTimeout(SERIAL_TIMEOUT);
    Serial.println("setup");
    //logicAnalyzer.setLogger(Serial);
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    printLine();
    testPins();
    testBufferSize();
    testSingleSample();

    for(auto &f : frequencies){
        testFrequency(f);
    }
    printLine();

    testFrequencyMaxSpeed();
    testPIO();
}

void loop() {
}