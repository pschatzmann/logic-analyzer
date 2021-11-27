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
#include "esp_int_wdt.h"


using namespace logic_analyzer;  

int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

LogicAnalyzer logicAnalyzer;
Capture capture(MAX_FREQ, MAX_FREQ_THRESHOLD);
TaskHandle_t task;

// when the status is changed to armed we start the capture
void captureHandler(void* ptr){
    Serial2.printf("captureHandler on core %d\n",xPortGetCoreID());
    // we use the led to indicate the capturing
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    while(true){
        if (logicAnalyzer.status() == ARMED){
            // start capture
            Serial2.println("capturing...");
            digitalWrite(LED_BUILTIN, HIGH);
            capture.capture();
            digitalWrite(LED_BUILTIN, LOW);
        }
        delay(10);
    }
}

void setup() {
    // setup logger
    Serial2.begin(115200, SERIAL_8N1);
    logicAnalyzer.setLogger(Serial2);

    // Setup Serial
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);

    logicAnalyzer.setDescription(DESCRIPTION);
    logicAnalyzer.setCaptureOnArm(false); 
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);

    // launch the capture handler on core 1
    int stack = 10000;
    int priority = 1;
    int core = 0;
    int ok = xTaskCreatePinnedToCore(captureHandler, "CaptureTask", stack, nullptr, priority, &task, core); 
    if(ok) {
        Serial2.println("Task created!");
    } else {
        Serial2.printf("Couldn't create task");
    }

}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
    delay(1);
}
