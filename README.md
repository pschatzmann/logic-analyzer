# Arduino Logic Analyzer

Recently when I started to research the topic of [Logic Analyzers](https://en.wikipedia.org/wiki/Logic_analyzer) I found the incredible [PulseView Project](https://sigrok.org/wiki/PulseView). However, I did not want to invest in additional hardware but just use one of my favorite microprocessors (ESP32, Raspberry Pico) as capturing device.

There are quite a few logic analyzer projects

- https://github.com/gillham/logic_analyzer
- https://github.com/gamblor21/rp2040-logic-analyzer/
- https://github.com/EUA/ESP32_LogicAnalyzer
- https://github.com/Ebiroll/esp32_sigrok

Howerver all of them are geared for one specific architecture and therfore are not portable.

I wanted to come up with a better design that clearly separates the generic functionality from the processor specific in order to support an easy rollout to new architectures: The only common precondition is the Arduino API. 

To come up with an optimized implementation for a specific board you just need to implement a specifig config.h and optionally you can also subclass LogicAnalyzer and overwirte e.g. your custom capture method...

