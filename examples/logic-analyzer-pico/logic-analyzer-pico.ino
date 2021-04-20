/**
 * @file logic_analyzer-pico.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for the sigrok LogicAnalyzer for the Rasperry Pico using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */

#include "Arduino.h"
#include "logic_analyzer.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"

using namespace logic_analyzer;  

LogicAnalyzer<PinBitArray> logicAnalyzer;
int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

// when the status is changed to armed we start the capture
void captureHandler(){
    // we use the led to indicate the capturing
    pinMode(LED_BUILTIN, OUTPUT);

    while(true){
        if (logicAnalyzer.status() == ARMED){
            // start capture
            digitalWrite(LED_BUILTIN, HIGH);
            logicAnalyzer.capture();
            digitalWrite(LED_BUILTIN, LOW);
        }
        // feed the dog
        watchdog_update();
    }
}

void setup() {
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    // switch off automatic capturing on arm command
    logicAnalyzer.setCaptureOnArm(false);

    // begin LogicAnalyzer
    logicAnalyzer.begin(Serial, new PinReader(pinStart), MAX_FREQ, MAX_FREQ_THRESHOLD, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    // launch the capture handler on core 1
    multicore_launch_core1(captureHandler);
}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}
