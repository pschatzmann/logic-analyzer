# AVR Processors

| Processor             | Max Freq | Max Samples | Pins | GPIO    |
|------------------------|---------|-------------|------|---------|
| AVR Processors (Nano)  |  109170 |         500 |   8  | GPIO0-7 |

Config file: [config_avr.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_avr.h)

Uses the portable, bit-banged `Capture` class - there is currently no
hardware-timed capture backend for AVR.

Note: `PinReader::readAll()` reads the `PINB`/`PIND` input registers (the
actual physical pin state), not `PORTB`/`PORTD` (the output latch), which
only reflects the real voltage for pins configured as `INPUT`.
