#pragma once
// Only the original ESP32 has the I2S0 "LCD/camera" registers this class
// relies on (conf2.lcd_en/camera_en) - ESP32-S2/S3/C3 have a different I2S
// peripheral and are not supported here.
#if defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32)

#include "Arduino.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "esp_intr_alloc.h"
#include "rom/lldesc.h"
#include "esp32/rom/gpio.h"
#include "soc/gpio_periph.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"

// Some logic to analyse:
#include "logic_analyzer.h"

namespace logic_analyzer {

// Global (single-instance) I2S DMA capture state - kept at namespace scope,
// like the rest of this library's singletons (pin_reader_ptr, buffer_ptr),
// so the interrupt handler below can be a plain function instead of a class
// member (member functions placed in per-TU COMDAT sections have caused
// "literal placed after use" link errors for IRAM_ATTR ISRs on some
// esp32-arduino core versions).
static volatile bool i2s_capture_dma_done = false;
static intr_handle_t i2s_capture_intr_handle = nullptr;

static void IRAM_ATTR i2s_capture_isr(void *arg) {
    if (I2S0.int_raw.in_suc_eof) {
        i2s_capture_dma_done = true;
        esp_intr_disable(i2s_capture_intr_handle);
        I2S0.conf.rx_start = 0;
    }
    I2S0.int_clr.val = I2S0.int_raw.val;
}

/**
 * @brief Hardware-timed 8 bit parallel GPIO capture for the original ESP32,
 * using the I2S0 peripheral's undocumented "camera" (LCD) mode + DMA - the
 * same trick used by the esp32-camera driver and by the EUA/ESP32_LogicAnalyzer
 * project (both credited below) to sample a parallel bus with zero CPU
 * involvement per sample.
 *
 * Unlike the RP2040 PIO, the ESP32's I2S0 can only be driven by an *external*
 * clock in this mode (I2S0.conf.rx_slave_mod = 1) - it does not sample on its
 * own internal clock. So this class generates the sample clock itself with
 * the LEDC PWM peripheral on #clk_out_pin and expects it to be looped back
 * with a jumper wire into #clk_in_pin, which is configured as the I2S input
 * clock. Default pins: clk_out=GPIO32 -> jumper -> clk_in=GPIO34.
 *
 * Data pins default to GPIO19..GPIO26 (8 bits), matching START_PIN/PIN_COUNT
 * in config_esp32.h.
 *
 * This capture path does not support triggering (same limitation as
 * PicoCapturePIO) - it always captures readCount samples starting immediately
 * on arm.
 *
 * Credit: register sequence adapted from
 * https://github.com/EUA/ESP32_LogicAnalyzer (GPLv3) and
 * https://github.com/igrr/esp32-cam-demo
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CaptureESP32I2S : public AbstractCapture {
    public:
        /// Default Constructor - uses GPIO19..GPIO26 as the 8 bit data bus
        CaptureESP32I2S(int pinStart = 19, int clkOutPin = 32, int clkInPin = 34) {
            clk_out_pin = clkOutPin;
            clk_in_pin = clkInPin;
            for (int j = 0; j < 8; j++) data_pins[j] = pinStart + j;
        }

        /// Defines the 8 data pins individually if they are not contiguous
        void setDataPins(const int pins[8]) {
            for (int j = 0; j < 8; j++) data_pins[j] = pins[j];
        }

        /// starts the capturing of the data
        void capture() override {
            log("capture()");
            start();
            waitForResult();
            dump();
            setStatus(STOPPED);
            log("Number of samples: %u", (unsigned) n_samples);
            log("Time in us: %lu", run_time_us);
        }

        /// Used to measure the speed - capture into memory w/o dump
        void captureAll() override {
            log("captureAll()");
            start();
            waitForResult();
        }

        /// Provides the measured capturing frequency (based on the last capture)
        float frequencyMeasured() {
            return run_time_us == 0 ? 0 : 1000000.0f * n_samples / run_time_us;
        }

    protected:
        int data_pins[8];
        int clk_out_pin;
        int clk_in_pin;
        uint32_t n_samples = 0;
        unsigned long start_time = 0;
        unsigned long run_time_us = 0;
        bool is_setup = false;

        static const size_t desc_size = 2000;  // bytes per DMA descriptor (must be a multiple of 4)
        lldesc_t *dma_desc = nullptr;
        uint8_t **dma_buf = nullptr;
        size_t dma_desc_count = 0;

        /// One-time GPIO/I2S peripheral setup
        void setupOnce() {
            if (is_setup) return;
            is_setup = true;

            int sig_data_base = I2S0I_DATA_IN0_IDX;
            for (int j = 0; j < 8; j++) gpioSetupIn(data_pins[j], sig_data_base + j);
            gpioSetupIn(clk_in_pin, I2S0I_WS_IN_IDX);
            // Signal 0x38 is the GPIO matrix's constant-1 input: tie HSYNC/VSYNC/HREF
            // permanently active so the "camera" logic samples continuously instead
            // of waiting for a real camera's frame/line sync signals.
            gpio_matrix_in(0x38, I2S0I_V_SYNC_IDX, false);
            gpio_matrix_in(0x38, I2S0I_H_SYNC_IDX, false);
            gpio_matrix_in(0x38, I2S0I_H_ENABLE_IDX, false);

            periph_module_enable(PERIPH_I2S0_MODULE);
            i2sConfReset();

            I2S0.conf.rx_slave_mod = 1;   // sampling clock is external (clk_in_pin)
            I2S0.conf2.val = 0;
            I2S0.conf2.lcd_en = 1;        // parallel ("LCD") mode
            I2S0.conf2.camera_en = 1;     // use HSYNC/VSYNC/HREF (tied high above) to gate sampling

            I2S0.clkm_conf.val = 0;
            I2S0.clkm_conf.clka_en = 0;
            I2S0.clkm_conf.clkm_div_a = 1;
            I2S0.clkm_conf.clkm_div_b = 0;
            I2S0.clkm_conf.clkm_div_num = 4;

            I2S0.fifo_conf.dscr_en = 1;
            I2S0.fifo_conf.rx_fifo_mod = 1;
            I2S0.fifo_conf.rx_fifo_mod_force_en = 1;
            I2S0.conf_chan.rx_chan_mod = 1;

            I2S0.conf.rx_right_first = 1;
            I2S0.conf.rx_msb_right = 0;
            I2S0.conf.rx_msb_shift = 0;
            I2S0.conf.rx_mono = 1;
            I2S0.conf.rx_short_sync = 1;
            I2S0.timing.val = 0;

            I2S0.sample_rate_conf.val = 0;
            I2S0.sample_rate_conf.rx_bits_mod = 8;  // 8 bit wide bus, matches PinBitArray
            I2S0.sample_rate_conf.rx_bck_div_num = 1;

            esp_intr_alloc(ETS_I2S0_INTR_SOURCE,
                            ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM,
                            &i2s_capture_isr, nullptr, &i2s_capture_intr_handle);
        }

        static void gpioSetupIn(int gpio, int sig) {
            if (gpio < 0) return;
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
            gpio_set_direction((gpio_num_t) gpio, GPIO_MODE_INPUT);
            gpio_matrix_in(gpio, sig, false);
        }

        static void i2sConfReset() {
            I2S0.lc_conf.in_rst = 1;  I2S0.lc_conf.in_rst = 0;
            I2S0.lc_conf.ahbm_rst = 1;  I2S0.lc_conf.ahbm_rst = 0;
            I2S0.lc_conf.ahbm_fifo_rst = 1;  I2S0.lc_conf.ahbm_fifo_rst = 0;
            I2S0.conf.rx_reset = 1;  I2S0.conf.rx_reset = 0;
            I2S0.conf.rx_fifo_reset = 1;  I2S0.conf.rx_fifo_reset = 0;
            while (I2S0.state.rx_fifo_reset_back) {}
        }

        /// (Re)allocates the DMA descriptor chain for the requested sample count
        void allocDma(uint32_t sampleCount) {
            freeDma();
            // Each 32 bit DMA word carries 2 samples (see dma_elem_t layout below),
            // so we need sampleCount/2 32-bit words, rounded up to whole descriptors.
            size_t total_bytes = ((sampleCount + 1) / 2) * 4;
            dma_desc_count = (total_bytes + desc_size - 1) / desc_size;
            if (dma_desc_count == 0) dma_desc_count = 1;

            dma_buf = (uint8_t **) malloc(sizeof(uint8_t *) * dma_desc_count);
            dma_desc = (lldesc_t *) malloc(sizeof(lldesc_t) * dma_desc_count);

            for (size_t i = 0; i < dma_desc_count; i++) {
                dma_buf[i] = (uint8_t *) malloc(desc_size);
                lldesc_t *pd = &dma_desc[i];
                pd->length = desc_size;
                pd->size = desc_size;
                pd->owner = 1;
                pd->sosf = 1;
                pd->buf = dma_buf[i];
                pd->offset = 0;
                pd->empty = 0;
                bool last = (i + 1 == dma_desc_count);
                pd->eof = last ? 1 : 0;
                pd->qe.stqe_next = last ? nullptr : &dma_desc[i + 1];
            }
        }

        void freeDma() {
            if (dma_buf != nullptr) {
                for (size_t i = 0; i < dma_desc_count; i++) free(dma_buf[i]);
                free(dma_buf);
                dma_buf = nullptr;
            }
            if (dma_desc != nullptr) {
                free(dma_desc);
                dma_desc = nullptr;
            }
            dma_desc_count = 0;
        }

        /// Drives the sample clock via LEDC PWM on clk_out_pin, looped back into clk_in_pin
        void enableSampleClock(uint32_t freqHz) {
            ledcAttach(clk_out_pin, freqHz, 1);
            ledcWrite(clk_out_pin, 1);
            delay(1);
        }

        void start() {
            setupOnce();
            n_samples = logicAnalyzer().readCount();
            allocDma(n_samples);
            // clear stale heap contents so a missing/too-slow sample clock (see class
            // comment) is reported as a flat 0 capture instead of leaking old heap data
            for (size_t i = 0; i < dma_desc_count; i++) memset(dma_buf[i], 0, desc_size);

            uint32_t freq = logicAnalyzer().captureFrequency();
            enableSampleClock(freq);

            i2s_capture_dma_done = false;
            i2sConfReset();
            I2S0.rx_eof_num = (n_samples + 1) / 2;  // in 32 bit words (2 samples/word)
            I2S0.in_link.addr = (uint32_t) &dma_desc[0];
            I2S0.in_link.start = 1;
            I2S0.int_clr.val = I2S0.int_raw.val;
            I2S0.int_ena.val = 0;
            I2S0.int_ena.in_suc_eof = 1;

            run_time_us = 0;
            start_time = micros();
            esp_intr_enable(i2s_capture_intr_handle);
            I2S0.conf.rx_start = 1;
        }

        void waitForResult() {
            log("waitForResult()");
            // the datasheet-documented rx_bck_div_num=1 corner case + slow clocks can
            // take a while; bound the wait so a bad frequency can't hang forever.
            unsigned long deadline = millis() + 5000;
            while (!i2s_capture_dma_done && millis() < deadline) delay(1);
            run_time_us = micros() - start_time;
            ledcDetach(clk_out_pin);

            RingBuffer &buf = logicAnalyzer().buffer();
            buf.clear();
            size_t written = 0;
            for (size_t i = 0; i < dma_desc_count && written < n_samples; i++) {
                uint8_t *b = dma_buf[i];
                // 32 bit DMA words hold two samples as 00 s2 00 s1 (see dma_elem_t in
                // the reference implementation) - byte 1 is the first sample, byte 3
                // (== byte 1 of the next word) is the second.
                for (size_t off = 0; off + 3 < desc_size && written < n_samples; off += 4) {
                    buf.write((PinBitArray) b[off + 1]);
                    written++;
                    if (written < n_samples) {
                        buf.write((PinBitArray) b[off + 3]);
                        written++;
                    }
                }
            }
            log("waitForResult() -> %u records", (unsigned) buf.available());
        }

        void dump() {
            size_t count = logicAnalyzer().available();
            write(logicAnalyzer().buffer().data_ptr(), count);
        }
};

} // namespace

#endif
