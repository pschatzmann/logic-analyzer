# A flexible Arduino SUMP Logic Analyzer Library

Recently, when I started to research the topic of [Logic Analyzers](https://en.wikipedia.org/wiki/Logic_analyzer), I found the incredible [PulseView Project](https://sigrok.org/wiki/PulseView). However, I did not want to invest in additional hardware but just use one of my favorite microprocessors (ESP32, Raspberry Pico) as capturing device.

There are quite a few logic analyzer projects with a similar goal:

- [gillham/logic_analyzer](https://github.com/gillham/logic_analyzer)
- [gamblor21/rp2040-logic-analyzer](https://github.com/gamblor21/rp2040-logic-analyzer)
- [EUA/ESP32_LogicAnalyzer](https://github.com/EUA/ESP32_LogicAnalyzer)
- [Ebiroll/esp32_sigrok](https://github.com/Ebiroll/esp32_sigrok)

Howerver all of them are geared for __one specific architecture__ and therfore are __not portable__.

I wanted to come up with a better design and provide a [simple Arduino C++ Library](https://pschatzmann.github.io/logic-analyzer/html/annotated.html) that implements the [SUMP protocol](https://www.sump.org/projects/analyzer/protocol/) and clearly separates the generic functionality from the processor specific in order to support an __easy rollout to new architectures__: The only common precondition is the Arduino API. 

I am currently providing implementations for

- AVR Processors
- ESP32
- ESP8266
- Raspberry Pico

# The Basic Arduino Sketch

The basic Arduino Sketch for the __logic-analyzer__ is quite simple. We just need create a [LogicAnalyzer](https://pschatzmann.github.io/logic-analyzer/html/classlogic__analyzer_1_1_logic_analyzer.html) and a [Capture](https://pschatzmann.github.io/logic-analyzer/html/classlogic__analyzer_1_1_capture.html) object.
In the setup we call the __begin method__ on the LogicAnalyzer object which provides all mandatory parameters. The provided implementation just uses the default values which are defined in the config.
Finally we add the __command handler__ in the loop():

```c++
#include "Arduino.h"
#include "logic_analyzer.h"

using namespace logic_analyzer;  

int pinStart=START_PIN;
int numberOfPins=PIN_COUNT;
LogicAnalyzer logicAnalyzer;
Capture capture(MAX_FREQ, MAX_FREQ_THRESHOLD);


void setup() {
    Serial.begin(SERIAL_SPEED);  
    Serial.setTimeout(SERIAL_TIMEOUT);
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, pinStart, numberOfPins);
}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}
```

# Adding Additional Functionality

## Logging

You can actvate the logging by assigning a Stream to the LogicAnalyzer object by calling logicAnalyzer.setLogger():

```c++
// setup logger
Serial1.begin(115200, SERIAL_8N1, 16, 17);
logicAnalyzer.setLogger(Serial1);
```

## Using Events

An easy way to extend the functionalty is by adding an event handler. The following acts on a status change event by activating the LED dependent on the actual status:

```c++
// Use Event handler to control the LED
void onEvent(Event event) {
    if (event == STATUS) {
        switch (logicAnalyzer.status()) {
            case ARMED:
                digitalWrite(LED_BUILTIN, LOW);
                break;
            case STOPPED:
                digitalWrite(LED_BUILTIN, LOW);
                break;
        }
    }
}
```

and we can just activate it by calling:

```c++
logicAnalyzer.setEventHandler(&onEvent);
```

## Custom Capturing

I am providing a default implementation for the capturing with the [Capture](https://pschatzmann.github.io/logic-analyzer/html/classlogic__analyzer_1_1_capture.html) class. It's main goal is portability because it should work on all Arduino Boards. To come up with a dedicated improved capturing is easy. Just implement your own class:

```c++
class YourFastCapture : public AbstractCapture {
    public:
        /// Default Constructor
        YourFastCapture() : AbstractCapture(){
        }

        /// starts the capturing of the data
        virtual void capture(){
            /// your implementation
        }
}
```

## Supporting new Architectures

In order to support a new architecture you need to implement a simple config file, that must contains the following information: 

- Defines for the __processor specific (resource) settings__ (e.g. MAX_CAPTURE_SIZE, SERIAL_SPEED ...)
- A __typedef of the PinBitArray__ which defines the recorded data size
- An implementation of the __class PinReader__ which reads all pins in one shot 

Here is the [config_esp32.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_esp32.h).


# Class Documentation

The complete [generated class documentation](https://pschatzmann.github.io/logic-analyzer/html/annotated.html) can be found on Github.


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

```shell
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/logic-analyzer.git
```


# Supported Boards


| Processor               | Max Freq  | Max Samples | Pins | GPIO      |
|-------------------------|-----------|-------------|------|-----------|
| ESP32                   |   2940052 |       65535 |   8  | GPIO19-26 |
| ESP8266                 |   1038680 |       50000 |   4  | GPIO12-15 |
| AVR Processors (Nano)   |    109170 |         500 |   8  | GPIO0-7   |
| Raspberry Pico          |   2508420 |       65535 |   8  | GPIO6-13  |
| Raspberry Pico - PIO    | 125000000 |       65535 |   8  | GPIO6-13  |


Please note, that SUMP supports only max 65535 samples.


# Summary

The basic implementation is only using a single core. While capturing is in process we do not support any cancellation triggered from Pulseview. In order to support this, we would just need to extend the functionality in a specific sketch to run the capturing on one core and the command handling on the second core. And this is exactly the purpose of this library: to be able to build a custom optimized logic analyzer implementation with minimal effort!

Please check out the [examples directory](https://github.com/pschatzmann/logic-analyzer/tree/main/examples) for some dedicated implementations. And if you come up with your own implementation, please share it with the community...
