/**
 * @file logic-analyzer-esp32s3-lcdcam.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Hardware-timed (LCD_CAM DVP controller + DMA) SUMP logic analyzer
 * for ESP32-S3 (and ESP32-P4).
 *
 * IMPORTANT WIRING: this peripheral needs an *external* pixel clock - you
 * MUST connect a jumper wire from GPIO18 (clock output) to GPIO17 (clock
 * input) for this to capture anything at all.
 *
 * Data pins (8 bits): GPIO1..GPIO8. vsync=GPIO15, de=GPIO16 (held statically
 * active via internal pull-ups, no wiring needed for those two).
 *
 * REQUIRED BOARD SETTINGS (Arduino IDE Tools menu / arduino-cli FQBN options):
 *   - PSRAM: "QSPI PSRAM" or "OPI PSRAM" (whichever matches your board) - the
 *     capture buffer must live in real, enabled PSRAM. Without it, capture
 *     fails safely (logged) but produces no data.
 *   - USB CDC On Boot: "Enabled" if your board's Serial is native USB (S3),
 *     otherwise SUMP commands over the USB port will never reach the sketch.
 *
 * Not supported yet: triggering (always captures readCount samples starting
 * immediately on arm).
 */
#include "Arduino.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This sketch only works on ESP32-S3 or ESP32-P4 (LCD_CAM peripheral)"
#endif

#include "capture_esp32s3_lcdcam.h"

using namespace logic_analyzer;

LogicAnalyzer logicAnalyzer;
CaptureESP32S3LcdCam capture(/*pinStart*/ 1, /*xclkPin*/ 18, /*pclkPin*/ 17,
                              /*vsyncPin*/ 15, /*dePin*/ 16);

void setup() {
    Serial.begin(SERIAL_SPEED);
    Serial.setTimeout(SERIAL_TIMEOUT);

    pinMode(LED_BUILTIN, OUTPUT);
    logicAnalyzer.setDescription("Arduino-ESP32S3-LCDCAM");
    logicAnalyzer.begin(Serial, &capture, MAX_CAPTURE_SIZE, 1, 8);
}

void loop() {
    if (Serial) logicAnalyzer.processCommand();
}
