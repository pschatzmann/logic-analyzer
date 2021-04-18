# Arduino Logic Analyzer

Recently, when I started to research the topic of [Logic Analyzers](https://en.wikipedia.org/wiki/Logic_analyzer), I found the incredible [PulseView Project](https://sigrok.org/wiki/PulseView). However, I did not want to invest in additional hardware but just use one of my favorite microprocessors (ESP32, Raspberry Pico) as capturing device.

There are quite a few logic analyzer projects with the same goal:

- https://github.com/gillham/logic_analyzer
- https://github.com/gamblor21/rp2040-logic-analyzer/
- https://github.com/EUA/ESP32_LogicAnalyzer
- https://github.com/Ebiroll/esp32_sigrok

Howerver all of them are geared for __one specific architecture__ and therfore are __not portable__.

I wanted to come up with a better design and provide a [basic C++ API](https://pschatzmann.github.io/logic-analyzer/html/annotated.html) that clearly separates the generic functionality from the processor specific in order to support an easy rollout to new architectures: The only common precondition is the Arduino API. 

To come up with an optimized implementation for a specific board you just need to implement a specifig config.h and optionally you can also subclass LogicAnalyzer and overwirte e.g. your custom capture method...

# The Arduino Sketch

The basic Arduino Sketch for a logic-analyzer is quite simple. We basically just need to call begin on a LogicAnalyzer object:
```
#include "Arduino.h"
#include "network.h"
#include "logic_analyzer.h"
#include "config_esp32.h"
#include "config_avr.h"

LogicAnalyzer logicAnalyzer;
int pinStart=4;
int numberOfPins=8;
int32_t maxCaptureSize=MAX_CAPTURE_SIZE;

void setup() {
    LOG_SETUP;
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    logicAnalyzer.setCaptureFrequency(MAX_FREQ);
    logicAnalyzer.begin(Serial, new PinReader(pinStart), maxCaptureSize, pinStart, numberOfPins);
}

void loop() {
    logicAnalyzer.processCommand();
}
```



# Connecting to Pulseview

- Start the Arduino logic-analyzer Sketch
- Start Pulseview
- Select Connect to a Device:
    - Choose the Driver: Openbentch Logic Sniffer & SUMP Compatibles
    - Choose the Interfarface: Select Serial Port and the Port to your Arduino Device with the frequency defined in the config<Device>.h (e.g. the ESP32 uses 921600)
    - Clock on "Scan for Devices using driver above" button
    - Select the Device - Arduino should be available and confirm with OK

