#pragma once
#ifdef ARDUINO_ARCH_RP2040

#include <stdio.h>
#include <stdlib.h>

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"

// Some logic to analyse:
#include "logic_analyzer.h"

namespace logic_analyzer {

/**
 * @brief First version of Capture implementation for Raspberry Pico using the PIO. Based on 
 * https://github.com/raspberrypi/pico-examples/blob/master/pio/logic_analyser/logic_analyser.c
 * 
 */
class PicoCapturePIO : public AbstractCapture {
    public:
        /// Default Constructor
        PicoCapturePIO() {
        }

        /// starts the capturing of the data
        virtual void capture(){
            start();
            print_capture_buf();
        }

        /// Used to test the speed
        unsigned long testCapture(float divider=1.0f){
            divider_value = divider;
            start();
            dma_channel_wait_for_finish_blocking(dma_chan);
            return micros() - start_time;
        }


        /// cancels the capturing which is ccurrently in progress
        void cancel() {
            if (!abort){
                abort = true;
                pio_sm_set_enabled(pio,  sm,  false);
                dma_channel_abort(dma_chan);
            }
        }
 
    protected:
        PIO pio = pio0;
        uint sm = 0;
        uint dma_chan = 0;

        uint pin_base;
        uint pin_count; 
        uint32_t n_samples;
        size_t capture_size_words;
        uint trigger_pin;
        bool trigger_level;
        float divider_value;
        uint64_t frequecy_value;
        bool abort = false;
        unsigned long start_time;


        void start() {
            // Get SUMP values 
            abort = false;
            pin_base = logicAnalyzer().startPin();
            pin_count = logicAnalyzer().numberOfPins();
            n_samples = logicAnalyzer().readCount();
            divider_value = divider(logicAnalyzer().captureFrequency());

            // Grant high bus priority to the DMA, so it can shove the processors out
            // of the way. This should only be needed if you are pushing things up to
            // >16bits/clk here, i.e. if you need to saturate the bus completely.
            bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

            pinMode(LED_BUILTIN, OUTPUT);
            digitalWrite(LED_BUILTIN, HIGH);

            logic_analyser_init();
            logic_analyser_arm();
        }

        float divider(uint32_t frequecy_value_hz){
            float result = 133000000.0f / float(frequecy_value_hz);
            log("divider: %f", result);
            return result;
        }

        void logic_analyser_arm() {
            log("Arming trigger");
            pio_sm_set_enabled(pio, sm, false);
            // Need to clear _input shift counter_, as well as FIFO, because there may be
            // partial ISR contents left over from a previous run. sm_restart does this.
            pio_sm_clear_fifos(pio, sm);
            pio_sm_restart(pio, sm);

            dma_channel_config c = dma_channel_get_default_config(dma_chan);
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            channel_config_set_transfer_data_size(&c, transferSize());
            channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

            dma_channel_configure(dma_chan, &c,
                logicAnalyzer().buffer().data_ptr(),        // Destination pointer
                &pio->rxf[sm],      // Source pointer
                n_samples, // Number of transfers
                true                // Start immediately
            );

            /// TODO proper trigger support
            ///pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
            start_time = micros();
            pio_sm_set_enabled(pio, sm, true);
        }

        dma_channel_transfer_size transferSize() {
            switch(sizeof(PinBitArray)){
                case 1:
                    return DMA_SIZE_8;
                case 2:
                    return DMA_SIZE_16;
                case 4:
                    return DMA_SIZE_32;
                default:
                    return DMA_SIZE_32;
            }
        }


        uint bit_count() {
            return sizeof(PinBitArray) * 8;
        }

        void logic_analyser_init() {
            log("Init trigger");
            // Load a program to capture n pins. This is just a single `in pins, n`
            // instruction with a wrap.
            uint16_t capture_prog_instr = pio_encode_in(pio_pins, pin_count);
            struct pio_program capture_prog = {
                    .instructions = &capture_prog_instr,
                    .length = 1,
                    .origin = -1
            };
            uint offset = pio_add_program(pio, &capture_prog);

            // Configure state machine to loop over this `in` instruction forever,
            // with autopush enabled.
            pio_sm_config c = pio_get_default_sm_config();
            sm_config_set_in_pins(&c, pin_base);
            sm_config_set_wrap(&c, offset, offset);
            sm_config_set_clkdiv(&c, divider_value);
            // Note that we may push at a < 32 bit threshold if pin_count does not
            // divide 32. We are using shift-to-right, so the sample data ends up
            // left-justified in the FIFO in this case, with some zeroes at the LSBs.
            sm_config_set_in_shift(&c, true, true, bit_count());
            sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
            pio_sm_init(pio, sm, offset, &c);
        }


        void print_capture_buf() {
            // wait for result an print it
            dma_channel_wait_for_finish_blocking(dma_chan);
            digitalWrite(LED_BUILTIN, LOW);

            // process result
            if (!abort){
                write(logicAnalyzer().buffer().data_ptr(), n_samples);
            } else {
                // unblock pulseview
                write(0);
            }
        }

};

} // namespace

#endif