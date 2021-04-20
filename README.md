# A flexible Arduino SUMP Logic Analyzer Library

Recently, when I started to research the topic of [Logic Analyzers](https://en.wikipedia.org/wiki/Logic_analyzer), I found the incredible [PulseView Project](https://sigrok.org/wiki/PulseView). However, I did not want to invest in additional hardware but just use one of my favorite microprocessors (ESP32, Raspberry Pico) as capturing device.

There are quite a few logic analyzer projects with a similar goal:

- [gillham/logic_analyzer](https://github.com/gillham/logic_analyzer)
- [gamblor21/rp2040-logic-analyzer](https://github.com/gamblor21/rp2040-logic-analyzer)
- [EUA/ESP32_LogicAnalyzer](https://github.com/EUA/ESP32_LogicAnalyzer)
- [Ebiroll/esp32_sigrok](https://github.com/Ebiroll/esp32_sigrok)

Howerver all of them are geared for __one specific architecture__ and therfore are __not portable__.

I wanted to come up with a better design and provide a [basic C++ Library](https://pschatzmann.github.io/logic-analyzer/html/annotated.html) that implements the [SUMP protocol](https://www.sump.org/projects/analyzer/protocol/) and clearly separates the generic functionality from the processor specific in order to support an __easy rollout to new architectures__: The only common precondition is the Arduino API. 

To come up with an basic implementation for a specific board you just need to implement a specific config. I am currently providing implementations for

- AVR Processors
- ESP32
- ESP8266
- Raspberry Pico

# An Example Processor Configuration File

The config file contains the following information: 

- Defines for the __processor specific (resource) settings__ (e.g. MAX_CAPTURE_SIZE, SERIAL_SPEED ...)
- Optionally the __logger__
- The __setupLogger() method__ which is used by the sketch (see next chapter)
- A __typedef of the PinBitArray__ which defines the recorded data size
- An implementation of the __class PinReader__ which reads all pins in one shot 

Here is the [config_esp32.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_esp32.h).


# The Arduino Sketch

The basic Arduino Sketch for the __logic-analyzer__ is quite simple. We just need to call the __begin method__ on a [LogicAnalyzer](https://pschatzmann.github.io/logic-analyzer/html/classlogic__analyzer_1_1_logic_analyzer.html) object and add the __command handler__ in the loop(). The provided implementation just uses the default values which are defined in the config:


```
#include "Arduino.h"
#include "logic_analyzer.h"

using namespace logic_analyzer;  

LogicAnalyzer<PinBitArray> logicAnalyzer;
int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;

void setup() {
    setupLogger(); // as defined in processor specific config
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    logicAnalyzer.begin(Serial, new PinReader(pinStart), MAX_FREQ, MAX_FREQ_THRESHOLD, MAX_CAPTURE_SIZE, pinStart, numberOfPins);
}

void loop() {
    logicAnalyzer.processCommand();
}
```


# Connecting to Pulseview

- Start the Arduino __"logic-analyzer"__ Sketch
- Start __Pulseview__
- Select "Connect to a Device":
    - Choose the Driver: __Openbentch Logic Sniffer & SUMP Compatibles__
    - Choose the Interface: Select __Serial Port__ with the Port to your Arduino Device and the frequency defined in the config<Device>.h (e.g. the ESP32 uses 921600)
    - Click on __"Scan for Devices using driver above"__ button
    - Select the Device - __"Arduino"__ which should be available and confirm with OK


# Installation

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with
```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/logic-analyzer.git
```


# Supported Boards

I have tested the functionality with the following processors:

|Processor               | Max Freq  | Max Samples | Pins |
|------------------------|-----------|-------------|------|
|ESP32                   |   2463700 |      100000 |    8 |
|ESP8266                 |   1038680 |       50000 |    4 |
|AVR Processors (Nano)   |    109170 |         500 |    8 |
|Raspberry Pico          |   2203225 |      100000 |    8 |


# Summary

The basic implementation is only using a single core. While capturing is in process we do not support any cancellation triggered from Pulseview. In order to support this, we would just need to extend the functionality in a specific sketch to run the capturing on one core and the command handling on the second core. And this is exactly the purpose of this library: to be able to build a custom optimized logic analyzer implementation with minimal effort!
