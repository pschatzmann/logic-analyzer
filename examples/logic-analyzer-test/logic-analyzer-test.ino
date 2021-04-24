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
float duty_cycle_percent = 60.0;

uint32_t frequencies[] = { 50000, 100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000, 10000000, 20000000,30000000,40000000,50000000,60000000,100000000, 500000000, 600000000  };

void printLine() {
    Serial.println("--------------------------");
}

void printOK(bool ok){
    Serial.println(ok ? " => OK " : " => ERROR");
}

void testBufferSize() {
    Serial.print("buffer size: ");
    Serial.print(logicAnalyzer.size());
    printOK(logicAnalyzer.size()>0);
    printLine();
}

void testSingleSample() {
    Serial.print("Caputre Single Sample: ");
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    capture.captureSampleFast();
    Serial.print(logicAnalyzer.available());
    printOK(logicAnalyzer.available() == 1);
    printLine();
}

float dutyCycle() {
    int count=0;
    while(logicAnalyzer.buffer().available()){
        PinBitArray ba = logicAnalyzer.buffer().read();
        if (ba & 0b00000001){
            count++;
        }
    }
    return 100.0 * count / logicAnalyzer.buffer().size();
}

/// we determine the effecitve measuring frequency and the effective duty cycle of the test signal
void testFrequency(AbstractCapture &capture_to_test, uint32_t frq){
    Serial.print("testing ");
    Serial.print(frq);
    logicAnalyzer.clear();
    logicAnalyzer.setCaptureFrequency(frq);
    logicAnalyzer.setStatus(TRIGGERED);
    uint64_t start = micros();
    capture_to_test.captureAll(); // captures maxCaptureSize samples w/o dump
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);
    
    Serial.print(" -> ");
    Serial.print(measured_freq);
    Serial.print(" using delay microsec ");
    Serial.print((uint32_t)logicAnalyzer.delayTimeUs());
    Serial.print(" duty cycle: ");
    float duty = dutyCycle(); 
    Serial.print(duty);
    float diff = abs(duty_cycle_percent - duty); 
    printOK(diff<3.0);


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

    Serial.print("duty cycle: ");
    float duty = dutyCycle(); 
    Serial.print(duty);
    float diff = abs(duty_cycle_percent - duty); 
    printOK(diff<2.0);
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

#ifdef ARDUINO_ARCH_RP2040

void testFrequencyPIO(PicoCapturePIO &capture_to_test, uint32_t frq){
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    logicAnalyzer.setCaptureFrequency(frq);

    Serial.print("testing ");
    Serial.print(frq);
    capture_to_test.captureAll(); // captures maxCaptureSize samples w/o dump
    Serial.print(" -> ");
    Serial.print(capture_to_test.frequencyMeasured());
    Serial.print(" hz / divider: ");
    Serial.print(capture_to_test.divider());
    Serial.print(" duty cycle: ");
    float duty = dutyCycle(); 
    Serial.print(duty);
    float diff = abs(duty_cycle_percent - duty); 
    printOK(diff<3.0);

}

void testPIO() {
    PicoCapturePIO picoCapture;
    logicAnalyzer.begin(Serial, &picoCapture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    Serial.println("testing PIO");
    Serial.print("max speed hz: ");
    Serial.println(picoCapture.maxFrequency());
    printLine();    

    for(auto &f : frequencies){
        testFrequencyPIO(picoCapture, f);
    }

    printLine();    
}

#endif    

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

    capture.activateTestSignal(logicAnalyzer.startPin(), duty_cycle_percent);
    testFrequencyMaxSpeed();
    for(auto &f : frequencies){
        testFrequency(capture, f);
    }
    printLine();

#ifdef ARDUINO_ARCH_RP2040
    testPIO();
#endif    

}

void loop() {
}