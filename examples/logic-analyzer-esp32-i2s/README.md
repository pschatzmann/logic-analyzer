## Logic Analyzer ESP32 I2S

This is a sketch which is implementing a hardware-timed Logic Analyser for the __original ESP32__, using the I2S0 peripheral in parallel "camera mode" with DMA.

- Data pins: GPIO19-26
- You __must__ connect a jumper wire from GPIO32 (clock output) to GPIO34 (clock input)
- Triggers are not supported yet
- Experimental: the captured data has not been verified with a real signal yet

See [docs/esp32.md](../../docs/esp32.md) for details.
