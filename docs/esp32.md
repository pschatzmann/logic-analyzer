# ESP32

## Supported Boards

| Variant                 | Max Freq  | Max Samples | Pins | GPIO      |
|--------------------------|-----------|-------------|------|-----------|
| ESP32 (software loop)    |  2940052* |       65535 |   8  | GPIO19-26 |
| ESP32 - I2S DMA          | untested  |       65535 |   8  | GPIO19-26 |

\* `MAX_FREQ` is the highest frequency offered to PulseView; the sampling
timing of the software loop is only reliable well below that (see below).

Default serial speed: 921600 baud (`SERIAL_SPEED`).

Config file: [config_esp32.h](https://github.com/pschatzmann/logic-analyzer/blob/main/src/config_esp32.h)

For the S3/P4-specific LCD_CAM backend, see [esp32s3.md](esp32s3.md).

## Software Loop Capture (`Capture`)

The default `Capture` class (see the [logic-analyzer](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer) and [logic-analyzer-esp32](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer-esp32) examples) samples pins in a bit-banged software loop. This is portable and simple, but gets unreliable above roughly a few tens of kHz: loop overhead, RTOS scheduling jitter, and WiFi/BT interrupts all eat directly into the sampling interval, distorting pulse widths at higher rates.

## Hardware-Timed Capture via I2S DMA (`CaptureESP32I2S`)

The [logic-analyzer-esp32-i2s](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer-esp32-i2s)
example (`CaptureESP32I2S`, in `src/capture_esp32_i2s.h`) instead drives the I2S0 peripheral's parallel
"camera mode" with DMA, so samples are captured with no CPU involvement,
similar in spirit to how `PicoCapturePIO` uses the RP2040's PIO block.

This only works on the __original ESP32__ (not S2/S3/C3, which have a
different I2S peripheral - see [esp32s3.md](esp32s3.md) for those), and the I2S0 peripheral needs an __external__
sampling clock in this mode - the sketch generates one itself via LEDC PWM
on GPIO32, which you must connect with a __jumper wire to GPIO34__ (the
clock input) for it to capture anything at all. If your data pins are not
contiguous you can define them individually with `setDataPins()`. Triggering is not
supported yet. Consider this experimental until confirmed against a real
captured signal.
