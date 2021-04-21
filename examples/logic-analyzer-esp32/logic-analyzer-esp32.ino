/**
 * @file logic_analyzer-pico.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Arduino Sketch for the sigrok LogicAnalyzer for the ESP32 using the SUMP protocol
 * See https://sigrok.org/wiki/Openbench_Logic_Sniffer#Short_Commands * 
 */

#ifndef ESP32
#error "This sketch is only for ESP32"
#endif

#include "Arduino.h"
#include "logic_analyzer.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // pin number is specific to your esp32 board
#endif

using namespace logic_analyzer;  

int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

LogicAnalyzer logicAnalyzer;
Capture capture;
TaskHandle_t task;

// when the status is changed to armed we start the capture
void captureHandler(void* ptr){
    // we use the led to indicate the capturing
    pinMode(LED_BUILTIN, OUTPUT);

    while(true){
        if (logicAnalyzer.status() == ARMED){
            // start capture
            digitalWrite(LED_BUILTIN, HIGH);
            capture.capture();
            digitalWrite(LED_BUILTIN, LOW);
        }
        delay(1);
    }
}

void setup() {
    // setup logger
    Serial1.begin(115200, SERIAL_8N1, 16, 17);
    logicAnalyzer.setLogger(LOG);

    // Setup Serial
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    
    // switch off automatic capturing on arm command
    logicAnalyzer.setCaptureOnArm(false);

    // begin LogicAnalyzer
    logicAnalyzer.begin(Serial, &capture, MAX_FREQ, MAX_FREQ_THRESHOLD, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    // launch the capture handler on core 1
    int stack = 10000;
    int priority = 0;
    int core = 1;
    xTaskCreatePinnedToCore(captureHandler, "CaptureTask", stack, NULL, priority, &task, core); 

}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}