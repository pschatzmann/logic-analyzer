# ESP32-S3 / ESP32-P4

For the original ESP32 (no LCD_CAM peripheral), see [esp32.md](esp32.md) instead.

## Hardware-Timed Capture via LCD_CAM/DVP DMA (`CaptureESP32S3LcdCam`)

The [logic-analyzer-esp32s3-lcdcam](https://github.com/pschatzmann/logic-analyzer/tree/main/examples/logic-analyzer-esp32s3-lcdcam)
example (`CaptureESP32S3LcdCam`, in `src/capture_esp32s3_lcdcam.h`) uses the
official `esp_driver_cam` DVP camera controller (the LCD_CAM peripheral +
DMA) to sample an 8 bit parallel GPIO bus with no CPU involvement per
sample - no real camera sensor is attached, GPIO levels are simply
captured as if they were pixel data. This is the S3/P4 equivalent of what
`CaptureESP32I2S` does for the original ESP32 via I2S0, and what
`PicoCapturePIO` does for the RP2040 via PIO.

Data pins default to GPIO1..GPIO8.

### Required wiring

Like the I2S0 trick on the original ESP32, this peripheral needs an
__external__ pixel clock - there is no self-clocked/master mode. The
driver generates that clock itself on GPIO18, which you __must__ loop
back with a jumper wire into GPIO17 (the clock input) for it to capture
anything at all. VSYNC (GPIO15) and DE (GPIO16) are held statically active
via internal pull-ups - no wiring needed for those two, since there's no
real camera sync signal to provide.

### Required board settings

- __PSRAM__: must be enabled ("QSPI PSRAM" or "OPI PSRAM", whichever
  matches your board) and the board must actually have a PSRAM chip. The
  driver's DMA path cache-syncs its buffer, which only works for cached
  external PSRAM - internal SRAM isn't cached at all. Without real,
  enabled PSRAM, capture fails safely (logged, no crash) but produces no
  data.
- __USB CDC On Boot__: "Enabled" on boards where `Serial` is native USB
  (S3) - otherwise SUMP commands sent over the USB port never reach the
  sketch at all.

### Known limitations

- No trigger support yet (same limitation as `PicoCapturePIO` /
  `CaptureESP32I2S`) - always captures `readCount()` samples immediately
  on arm.
- This is a single-shot capture (one "frame" per SUMP ARM), not
  continuous video.
- The h_res/v_res/vsync semantics of this driver are normally driven by a
  real camera sensor; tying vsync/de statically high instead of letting
  them pulse per frame/line is only verified up to safe bring-up (device
  detection, correct zero-sample response with no clock connected, no
  crashes) - the actual captured sample content with the clock loopback
  wire in place is still unverified.
