# AVR Processors

| Processor             | Max Freq | Max Samples | Pins | GPIO    |
|------------------------|---------|-------------|------|---------|
| AVR Processors (Nano)  |  109170 |         500 |   8  | GPIO0-7 |
| Arduino Mega 2560      |         |         500 |   8  | GPIO22-29 |

Config file: [config_avr.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_avr.h)

Uses the portable, bit-banged `Capture` class - there is currently no
hardware-timed capture backend for AVR.

Note: `PinReader::readAll()` reads the `PINB`/`PIND` input registers (the
actual physical pin state), not `PORTB`/`PORTD` (the output latch), which
only reflects the real voltage for pins configured as `INPUT`.

On the Arduino Mega 2560 the GPIO numbers map to different port registers
than on the Uno/Nano, so a separate `PinReader` is used: the default start pin
is 22 (pins 22-29 = `PINA`). Start pins A0-A7 (`PINF`) and A8-A15 (`PINK`) are
also read directly from the port register; any other start pin falls back to
the (slower) `digitalRead()`.

The default serial speed is 9600 baud (`SERIAL_SPEED`): make sure you select
the same baud rate in PulseView, otherwise the device will not be detected.
