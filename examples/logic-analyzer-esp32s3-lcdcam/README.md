## Logic Analyzer ESP32-S3 / ESP32-P4 LCD_CAM

This is a sketch which is implementing a hardware-timed Logic Analyser for the __ESP32-S3__ and __ESP32-P4__, using the LCD_CAM (DVP camera) peripheral with DMA.

- Data pins: GPIO1-8
- You __must__ connect a jumper wire from GPIO18 (clock output) to GPIO17 (clock input)
- PSRAM must be enabled, and on boards with native USB "USB CDC On Boot" must be enabled
- Triggers are not supported yet
- Experimental: the captured data has not been verified with a real signal yet

See [docs/esp32s3.md](../../docs/esp32s3.md) for details.
