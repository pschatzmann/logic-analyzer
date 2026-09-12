#pragma once
// Only chips with the LCD_CAM peripheral's DVP camera controller support this
// (ESP32-S3, ESP32-P4). The original ESP32 does not have LCD_CAM at all - use
// capture_esp32_i2s.h (I2S0 "camera mode") for that chip instead.
#if defined(ESP32) && (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4))

#include "Arduino.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_heap_caps.h"

// Some logic to analyse:
#include "logic_analyzer.h"

namespace logic_analyzer {

// Global (single-instance) capture state for the callback-based DVP driver -
// see the equivalent note in capture_esp32_i2s.h for why these are plain
// namespace-scope globals rather than class members.
static volatile bool s3cam_trans_done = false;
static uint8_t *s3cam_buffer = nullptr;
static size_t s3cam_buflen = 0;

static bool IRAM_ATTR s3cam_on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data) {
    trans->buffer = s3cam_buffer;
    trans->buflen = s3cam_buflen;
    return false;
}

static bool IRAM_ATTR s3cam_on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data) {
    s3cam_trans_done = true;
    return false;
}

/**
 * @brief Hardware-timed 8 bit parallel GPIO capture for ESP32-S3/P4, using the
 * official esp_driver_cam "DVP" camera controller (LCD_CAM peripheral + DMA)
 * with no real camera sensor attached - i.e. capturing raw parallel GPIO
 * levels as if they were pixel data.
 *
 * Like capture_esp32_i2s.h's I2S0 trick on the original ESP32, this
 * peripheral needs an *external* pixel clock (PCLK) - there is no
 * self-clocked/master mode. The driver can generate that clock itself on
 * #xclk_pin, which you MUST loop back with a jumper wire into #pclk_pin.
 * #vsync_pin and #de_pin are held statically active via internal pull-ups
 * (no real camera sync signals are needed) so the controller just streams
 * continuously for as long as PCLK ticks.
 *
 * This is a single-shot capture (one "frame" of readCount() bytes per SUMP
 * ARM), not continuous video. No trigger support (same limitation as
 * PicoCapturePIO / CaptureESP32I2S).
 *
 * REQUIRES PSRAM (real chip present, and enabled in board settings): the
 * driver's DMA path cache-syncs its buffer, which only works for cached
 * external PSRAM, not internal SRAM - without it, capture fails safely
 * (logged, no crash) instead of producing data.
 *
 * EXPERIMENTAL: the h_res/v_res/vsync semantics of this driver are normally
 * driven by a real camera sensor; tying vsync/de statically high instead of
 * letting them pulse per frame/line is untested beyond basic bring-up.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CaptureESP32S3LcdCam : public AbstractCapture {
    public:
        /// Default Constructor - data bus GPIO1..GPIO8, xclk=18, pclk=17, vsync=15, de=16
        CaptureESP32S3LcdCam(int pinStart = 1, int xclkPin = 18, int pclkPin = 17,
                              int vsyncPin = 15, int dePin = 16) {
            xclk_pin = xclkPin;
            pclk_pin = pclkPin;
            vsync_pin = vsyncPin;
            de_pin = dePin;
            for (int j = 0; j < 8; j++) data_pins[j] = pinStart + j;
        }

        /// Defines the 8 data pins individually if they are not contiguous
        void setDataPins(const int pins[8]) {
            for (int j = 0; j < 8; j++) data_pins[j] = pins[j];
        }

        void capture() override {
            log("capture()");
            start();
            waitForResult();
            dump();
            setStatus(STOPPED);
            log("Number of samples: %u", (unsigned) n_samples);
            log("Time in us: %lu", run_time_us);
        }

        void captureAll() override {
            log("captureAll()");
            start();
            waitForResult();
        }

        float frequencyMeasured() {
            return run_time_us == 0 ? 0 : 1000000.0f * n_samples / run_time_us;
        }

    protected:
        int data_pins[8];
        int xclk_pin, pclk_pin, vsync_pin, de_pin;
        esp_cam_ctlr_handle_t handle = nullptr;
        esp_cam_ctlr_dvp_pin_config_t pin_cfg{};
        bool is_setup = false;
        uint32_t configured_freq = 0;
        uint32_t configured_samples = 0;
        uint32_t n_samples = 0;
        unsigned long start_time = 0;
        unsigned long run_time_us = 0;

        /// (Re)creates the controller - needs to be recreated whenever the
        /// requested sample rate (xclk_freq) changes since it's fixed at
        /// esp_cam_new_dvp_ctlr() time.
        void setupFor(uint32_t freqHz) {
            if (is_setup && configured_freq == freqHz && configured_samples == n_samples) return;
            if (!psramFound()) {
                // Avoid even attempting esp_cam_ctlr_alloc_buffer(): the driver
                // logs its own ESP_LOGE line straight to the SUMP stream (same
                // USB CDC port) before returning NULL, which we can't suppress.
                log("No PSRAM found - CaptureESP32S3LcdCam requires a board with PSRAM enabled");
                handle = nullptr;
                is_setup = false;
                return;
            }
            if (is_setup) {
                esp_cam_ctlr_disable(handle);
                esp_cam_ctlr_del(handle);
                handle = nullptr;
                is_setup = false;
            }
            configured_freq = freqHz;
            configured_samples = n_samples;

            // No real camera: hold DE/VSYNC statically active via pull-ups
            // instead of wiring real sync signals.
            pinMode(vsync_pin, INPUT_PULLUP);
            pinMode(de_pin, INPUT_PULLUP);

            pin_cfg = {};
            pin_cfg.data_width = CAM_CTLR_DATA_WIDTH_8;
            for (int j = 0; j < 8; j++) pin_cfg.data_io[j] = (gpio_num_t) data_pins[j];
            for (int j = 8; j < CAM_DVP_DATA_SIG_NUM; j++) pin_cfg.data_io[j] = GPIO_NUM_NC;
            pin_cfg.vsync_io = (gpio_num_t) vsync_pin;
            pin_cfg.de_io = (gpio_num_t) de_pin;
            pin_cfg.pclk_io = (gpio_num_t) pclk_pin;
            pin_cfg.xclk_io = (gpio_num_t) xclk_pin;

            esp_cam_ctlr_dvp_config_t cfg{};
            cfg.ctlr_id = 0;
            cfg.clk_src = CAM_CLK_SRC_DEFAULT;
            cfg.h_res = n_samples > 0 ? n_samples : 1;
            cfg.v_res = 1;
            cfg.input_data_color_type = CAM_CTLR_COLOR_RAW8;
            cfg.cam_data_width = 8;
            cfg.bk_buffer_dis = 1;  // we supply the buffer ourselves via the callback below
            cfg.dma_burst_size = 0;
            cfg.xclk_freq = freqHz;
            cfg.pin = &pin_cfg;

            esp_err_t err = esp_cam_new_dvp_ctlr(&cfg, &handle);
            if (err != ESP_OK) {
                log("esp_cam_new_dvp_ctlr failed: %d", (int) err);
                handle = nullptr;
                return;
            }

            esp_cam_ctlr_evt_cbs_t cbs{};
            cbs.on_get_new_trans = s3cam_on_get_new_trans;
            cbs.on_trans_finished = s3cam_on_trans_finished;
            esp_cam_ctlr_register_event_callbacks(handle, &cbs, nullptr);
            esp_cam_ctlr_enable(handle);

            // Must use the controller's own allocator, not plain heap_caps_malloc,
            // AND it must come from PSRAM (MALLOC_CAP_SPIRAM), not internal SRAM:
            // this driver cache-syncs the buffer before/after DMA (esp_cache_msync),
            // which only applies to cached external PSRAM - internal SRAM isn't
            // cached at all, so esp_cache_msync() on it just fails, corrupting the
            // SUMP byte stream (both go out over the same USB CDC port). This means
            // a board WITH real, enabled PSRAM is a hard requirement for this class.
            // esp_cache_msync() also requires the buffer *size* (not just its
            // address) to be a multiple of the cache line size (32 bytes here) -
            // round up, but only copy out the real n_samples bytes afterwards.
            static const size_t cache_line = 32;
            size_t alloc_len = ((n_samples + cache_line - 1) / cache_line) * cache_line;
            if (s3cam_buffer != nullptr) free(s3cam_buffer);
            s3cam_buffer = (uint8_t *) esp_cam_ctlr_alloc_buffer(handle, alloc_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
            s3cam_buflen = alloc_len;
            if (s3cam_buffer == nullptr) {
                // Without a buffer, esp_cam_ctlr_start()/the driver's internal
                // on_get_new_trans handling hits a hard assert() and reboots -
                // bail out here instead so a missing/disabled PSRAM fails safely.
                log("esp_cam_ctlr_alloc_buffer failed - is PSRAM present and enabled?");
                esp_cam_ctlr_disable(handle);
                esp_cam_ctlr_del(handle);
                handle = nullptr;
                return;
            }

            is_setup = true;
        }

        void start() {
            n_samples = logicAnalyzer().readCount();
            uint32_t freq = logicAnalyzer().captureFrequency();
            if (freq == 0) freq = 1000000;
            setupFor(freq);
            if (handle == nullptr) return;

            s3cam_trans_done = false;

            run_time_us = 0;
            start_time = micros();
            esp_cam_ctlr_start(handle);
        }

        void waitForResult() {
            log("waitForResult()");
            if (handle == nullptr || s3cam_buffer == nullptr) return;
            unsigned long deadline = millis() + 5000;
            while (!s3cam_trans_done && millis() < deadline) delay(1);
            run_time_us = micros() - start_time;
            esp_cam_ctlr_stop(handle);

            RingBuffer &buf = logicAnalyzer().buffer();
            buf.clear();
            size_t count = s3cam_trans_done ? n_samples : 0;
            for (size_t i = 0; i < count; i++) buf.write((PinBitArray) s3cam_buffer[i]);
            log("waitForResult() -> %u records", (unsigned) buf.available());
        }

        void dump() {
            size_t count = logicAnalyzer().available();
            write(logicAnalyzer().buffer().data_ptr(), count);
        }
};

} // namespace

#endif
