# Arduino Logic Analyzer

Recently, when I started to research the topic of [Logic Analyzers](https://en.wikipedia.org/wiki/Logic_analyzer), I found the incredible [PulseView Project](https://sigrok.org/wiki/PulseView). However, I did not want to invest in additional hardware but just use one of my favorite microprocessors (ESP32, Raspberry Pico) as capturing device.

There are quite a few logic analyzer projects with a similar goal:

- https://github.com/gillham/logic_analyzer
- https://github.com/gamblor21/rp2040-logic-analyzer/
- https://github.com/EUA/ESP32_LogicAnalyzer
- https://github.com/Ebiroll/esp32_sigrok

Howerver all of them are geared for __one specific architecture__ and therfore are __not portable__.

I wanted to come up with a better design and provide a [basic C++ API](https://pschatzmann.github.io/logic-analyzer/html/annotated.html) that clearly separates the generic functionality from the processor specific in order to support an easy rollout to new architectures: The only common precondition is the Arduino API. 

To come up with an optimized implementation for a specific board you just need to implement a specifig config.h and optionally you can also subclass LogicAnalyzer and overwirte e.g. your custom capture method...

# The Arduino Sketch

The basic Arduino Sketch for a logic-analyzer is quite simple. We basically just need to call begin on a LogicAnalyzer object and add the command handler in the loop():

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



# Supported Archtectures

I have tested the functionality with the following processors:

|Processor               | Max Freq  | Max Samples | Pins |
|------------------------|-----------|-------------|------|
|ESP32                   |   2463700 |      100000 |    8 |
|ESP8266                 |   1038680 |       50000 |    4 |
|AVR Processors (Nano)   |    109170 |         500 |    8 |
|Raspberry Pico          |           |             |      |


# Limitations / Potential Improvements

The basic implementation is using a single core. While capturing is in process we do not support any cancellation triggered from Pulseview.
In order to support this, we would need to extend the functionality to run the capturing on one core and the command handling on the second core.
