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
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class PicoCapturePIO : public AbstractCapture {
    public:
        /// Default Constructor
        PicoCapturePIO() {
        }

        /// starts the capturing of the data
        virtual void capture(){
            log("capture()");
            start();
            dump();
            // signal end of processing
            setStatus(STOPPED);

            log("Number of samples: %d", n_samples);
            log("Time in us: %lu", run_time_us);
            log("Measured capturing frequency in Hz is %f", frequencyMeasured());
        }

        /// cancels the capturing which is ccurrently in progress
        void cancel() {
            log("cancel()");
            if (!abort){
                abort = true;
                pio_sm_set_enabled(pio,  sm,  false);
                dma_channel_abort(dma_chan);
            }
        }

        /// Used to test the speed
        virtual void captureAll(){
            log("captureAll()");
            start();
            waitForResult();
        }

        /// provides the runtime in microseconds from the capturing start to when the data is available
        unsigned long runtimeUs(){
            return run_time_us;
        }

        /// Provides the measured capturing frequency
        float frequencyMeasured(){
            float measured_freq = run_time_us == 0 ? 0 : 1000000.0 * n_samples  / run_time_us;
            return measured_freq;
        }

        float maxFrequency(int warmup=1, int repeat=2) {
            log("determine maxFrequency");
            if (max_frequecy_value<=0.0) {
                pin_base = logicAnalyzer().startPin();
                pin_count = logicAnalyzer().numberOfPins();
                n_samples = logicAnalyzer().readCount();
                divider_value = 1.0;

                // warm up
                for (int j=0;j<warmup;j++){
                    arm();
                    dma_channel_wait_for_finish_blocking(dma_chan);
                }

                // measure
                float freqTotal = 0;
                for (int j=0;j<repeat;j++){
                    arm();
                    dma_channel_wait_for_finish_blocking(dma_chan);
                    run_time_us = micros() - start_time;
                    freqTotal += frequencyMeasured();
                }
                max_frequecy_value = freqTotal / repeat;
                log("maxFrequency: %f", max_frequecy_value);
            }
            return max_frequecy_value;
        }

        float divider() {
            return divider_value;
        }


    protected:
        PIO pio = pio0;
        uint sm = 0;
        uint dma_chan = 0;

        uint pin_base;
        uint pin_count; 
        uint32_t n_samples;
        uint32_t n_transfers;
        size_t capture_size_words;
        uint trigger_pin;
        bool trigger_level;
        float divider_value;
        uint64_t frequecy_value;
        float max_frequecy_value = -1.0;  // in hz
        bool abort = false;
        unsigned long start_time;
        unsigned long run_time_us;
        float test_duty_cycle;
        int test_pin=-1;

        /// starts the processing
        void start() {
            log("start()");
            // if we are well above the limit we do not capture at all
            if (logicAnalyzer().captureFrequency() > (1.5 * maxFrequency())){
                setStatus(STOPPED);
                // Send some dummy data to stop pulseview
                write(0);
                log("The frequency %u is not supported!", logicAnalyzer().captureFrequency () );
                return;
            }

            // Get SUMP values 
            abort = false;
            pin_base = logicAnalyzer().startPin();
            pin_count = logicAnalyzer().numberOfPins();
            n_samples = logicAnalyzer().readCount();
            divider_value = calculateDivider(logicAnalyzer().captureFrequency());

            arm();
        }


        /// determines the divider value 
        float calculateDivider(uint32_t frequecy_value_hz){
            // 1.0 => maxCaptureFrequency()
            float result = static_cast<float>(maxFrequency()) / static_cast<float>(frequecy_value_hz) ;
            log("divider: %f", result);
            return result < 1.0 ? 1.0 : result;
        }

        /// intitialize the PIO
        void arm() {
            log("arm()");

            log("- Init trigger");

            // Grant high bus priority to the DMA, so it can shove the processors out
            // of the way. This should only be needed if you are pushing things up to
            // >16bits/clk here, i.e. if you need to saturate the bus completely.
            bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

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
            sm_config_set_in_shift(&c, true, true, 32);
            sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
            pio_sm_init(pio, sm, offset, &c);

            /// arms the logic analyzer
            log("- Arming trigger");
            pio_sm_set_enabled(pio, sm, false);
            // Need to clear _input shift counter_, as well as FIFO, because there may be
            // partial ISR contents left over from a previous run. sm_restart does this.
            pio_sm_clear_fifos(pio, sm);
            pio_sm_restart(pio, sm);

            dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
            channel_config_set_read_increment(&dma_config, false);
            channel_config_set_write_increment(&dma_config, true);
            channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
            channel_config_set_dreq(&dma_config, pio_get_dreq(pio, sm, false));

            n_transfers = n_samples /4 * sizeof(PinBitArray);
            dma_channel_configure(dma_chan, &dma_config,
                logicAnalyzer().buffer().data_ptr(),        // Destination pointer
                &pio->rxf[sm],      // Source pointer
                n_transfers,        // Number of transfers
                true                // Start immediately
            );

            /// TODO proper trigger support
            ///pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));

            run_time_us = 0;
            start_time = micros();
            pio_sm_set_enabled(pio, sm, true);
        }

        // /// Determines the dma channel tranfer size
        // dma_channel_transfer_size transferSize(int bytes) {
        //     switch(bytes){
        //         case 1:
        //             return DMA_SIZE_8;
        //         case 2:
        //             return DMA_SIZE_16;
        //         case 4:
        //             return DMA_SIZE_32;
        //         default:
        //             return DMA_SIZE_32;
        //     }
        // }


        // /// determines the number of bits 
        // uint bit_count() {
        //     return sizeof(PinBitArray) * 8;
        // }

        /// Dumps the result to PuleView (SUMP software)
        void dump() {
            log("dump()");
            // wait for result an print it
            waitForResult();
            // process result
            if (!abort){
                size_t count = logicAnalyzer().available();
                write(logicAnalyzer().buffer().data_ptr(), count);
                log("dump() - ended with %u records", count);
            } else {
                // unblock pulseview
                write(0);
                log("dump() - aborted");
            }
        }

        /// Wait for result and update run_time_us and buffer available
        void waitForResult() {
            log("waitForResult()");
            dma_channel_wait_for_finish_blocking(dma_chan);
            run_time_us = micros() - start_time;
            size_t record_count = n_transfers * 4 / sizeof(PinBitArray);
            logicAnalyzer().buffer().setAvailable(abort ? 0 : record_count);
            log("waitForResult() -> result available with %u records",record_count);
        }

};

} // namespace

#endif