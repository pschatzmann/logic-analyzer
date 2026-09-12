# Raspberry Pi Pico / RP2040 / RP2350

| Variant                 | Max Freq  | Max Samples | Pins | GPIO      |
|-------------------------|-----------|-------------|------|-----------|
| Raspberry Pico          |   2508420 |       65535 |   8  | GPIO6-13  |
| Raspberry Pico - PIO    | 125000000 |       65535 |   8  | GPIO6-13  |

Config file: [config_pico.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_pico.h)

## Software Loop Capture (`Capture`)

The [logic-analyzer-pico](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer-pico)
example uses the portable, bit-banged `Capture` class - simple, but limited
in top sampling rate like on any other board using it.

## Hardware-Timed Capture via PIO + DMA (`PicoCapturePIO`)

The [logic-analyzer-pico-pio](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer-pico-pio)
example (`PicoCapturePIO`, in `src/capture_raspberry_pico.h`) drives the
RP2040/RP2350's PIO block with DMA, capturing samples with no CPU
involvement per sample and reaching much higher sustained rates than the
software loop. No trigger support yet.
