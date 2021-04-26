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

#define REGULAR_TEST
#ifdef ARDUINO_ARCH_RP2040
#define TEST_PIO
#endif

using namespace logic_analyzer;  

float duty_cycle_percent = 60.0;
int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;
LogicAnalyzer logicAnalyzer;
Capture capture(MAX_FREQ, MAX_FREQ_THRESHOLD);
#ifdef TEST_PIO
PicoCapturePIO capturePIO;
#endif 

uint64_t frequencies[] = { 50000, 100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000, 10000000, 20000000,30000000,40000000,50000000,60000000,100000000lu, 500000000lu, 600000000lu  };

// just prints a line
void printLine() {
    Serial.println("-------------------------------------");
}

/// Prints OK or ERROR
void printOK(bool ok){
    Serial.println(ok ? " => OK " : " => ERROR");
}

// test buffer
void testBufferSize(LogicAnalyzer &logicAnalyzer) {
    Serial.print("buffer size: ");
    Serial.print(logicAnalyzer.size());
    printOK(logicAnalyzer.size()>0);
    printLine();
}

/// Generates a test PWM signal
void activateTestSignal(int testPin, float dutyCyclePercent) {
    if (testPin>=0){
        log("Starting PWM test signal with duty %f %", dutyCyclePercent);
        pinMode(testPin, OUTPUT);
        int value = dutyCyclePercent / 100.0 * 255.0;
        analogWrite(testPin, value);
    }
}


// Test all pins
void testPins(LogicAnalyzer &logicAnalyzer, Capture &capture) {
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
        printOK((result >> (pin-pinStart)) & 1);
    }
    printLine();
}

// test a single sample
void testSingleSample(LogicAnalyzer &logicAnalyzer, Capture &capture) {
    Serial.print("Caputre Single Sample: ");
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);
    capture.captureSampleFast();
    Serial.print(logicAnalyzer.available());
    printOK(logicAnalyzer.available() == 1);
    printLine();
}

// calculate the duty cycle from the captured data
float dutyCycle(LogicAnalyzer &logicAnalyzer, PinBitArray pinFilter) {
    int count=0;
    logic_analyzer::RingBuffer *rb = &logicAnalyzer.buffer();
    while(rb->available()){
        PinBitArray ba = rb->read();
        //Serial.print(ba, BIN);
        //Serial.print(" ");
        if (ba & pinFilter != 0){
            count++;
        }
    }
    return 100.0 * count / logicAnalyzer.buffer().size();
}

/// we determine the effecitve measuring frequency and the effective duty cycle of the test signal
void testFrequency(LogicAnalyzer &logicAnalyzer, AbstractCapture &capture_to_test, uint64_t frq){
    Serial.print("Testing ");
    Serial.print(frq);
    logicAnalyzer.clear();
    logicAnalyzer.setCaptureFrequency(frq);
    logicAnalyzer.setStatus(TRIGGERED);

    // time capturing
    uint64_t start = micros();
    capture_to_test.captureAll(); // captures maxCaptureSize samples w/o dump
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);
    float duty = dutyCycle(logicAnalyzer, 0b00000001); 
    float diff = abs(duty_cycle_percent - duty); 
    
    Serial.print(" -> ");
    Serial.print(measured_freq);
    Serial.print(" hz / delay us ");
    Serial.print((uint32_t)logicAnalyzer.delayTimeUs());
    Serial.print(" / duty cycle: ");
    Serial.print(duty);
    printOK(diff<3.0);
}

// Test using max speed implementation
void testFrequencyMaxSpeed(LogicAnalyzer &logicAnalyzer, Capture &capture){
    Serial.println("Testing max speed ");
    logicAnalyzer.clear();
    logicAnalyzer.setStatus(TRIGGERED);

    // time capturing
    uint64_t start = micros();
    capture.captureAllMaxSpeed();
    uint64_t end = micros();

    float measured_freq = 1000000.0 * logicAnalyzer.available()  / (end - start);
    size_t available = logicAnalyzer.available();
    float duty = dutyCycle(logicAnalyzer,0b00000001); 
    float diff = abs(duty_cycle_percent - duty); 

    Serial.print("time us: ");
    Serial.println((double) (end - start));
    Serial.print("sample count: ");
    Serial.println(available);
    Serial.print("max speed: ");
    Serial.println(measured_freq);
    Serial.print("duty cycle: ");
    Serial.print(duty);
    printOK(diff<2.0);
    printLine();
}

/// test for all non pio tests
void testAll() {
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    printLine();
    testPins(logicAnalyzer, capture);
    testBufferSize(logicAnalyzer);
    testSingleSample(logicAnalyzer, capture);

    activateTestSignal(logicAnalyzer.startPin(), duty_cycle_percent);
    delay(100);
    testFrequencyMaxSpeed(logicAnalyzer, capture);
    for(auto &f : frequencies){
        testFrequency(logicAnalyzer, capture, f);
    }
    printLine();

}

#ifdef TEST_PIO

// Test single frequency using the PIO
float testFrequencyPIO(LogicAnalyzer &logicAnalyzer, PicoCapturePIO &capture_to_test, uint64_t frq){
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
    Serial.print(" / duty cycle: ");
    float duty = dutyCycle(logicAnalyzer, 0b100); 
    Serial.print(duty);
    float diff = abs(duty_cycle_percent - duty); 
    printOK(diff<3.0);
    return duty;

}

// All tests for the Raspberry PI PIO
void testAllPIO() {
    logicAnalyzer.begin(Serial, &capturePIO, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    testBufferSize(logicAnalyzer);

    Serial.println("testing PIO");
    Serial.print("max speed hz: ");
    Serial.println(capturePIO.maxFrequency());
    printLine();    

    activateTestSignal(logicAnalyzer.startPin()+2, duty_cycle_percent);
    for(auto &f : frequencies){
        delay(200);
        testFrequencyPIO(logicAnalyzer, capturePIO, f);
    }

    printLine();    
}

#endif    

void setup() {
    Serial.begin(115200);  
    //Logger.begin(Serial, PicoLogger::Debug);
    //logicAnalyzer.setLogger(Serial);

    // wait for Serial to be ready
    while(!Serial);
    Serial.setTimeout(SERIAL_TIMEOUT);
    Serial.println("setup");

#ifdef REGULAR_TEST
    testAll();
#endif

#ifdef TEST_PIO
    testAllPIO();
#endif    

}

void loop() {
}