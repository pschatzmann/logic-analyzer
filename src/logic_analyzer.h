/**
 * @file logic_analyzer_generic.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Generic implementation of SUMP based logic analyzer
 * 
 */
#pragma once

#include "Arduino.h"

#ifndef PINS_TYPE
#define PINS_TYPE uint8_t
#endif

#define LOG_BUFFER_SIZE 80

// Supported Commands
#define SUMP_RESET 0x00
#define SUMP_ARM   0x01
#define SUMP_ID    0x02
#define SUMP_XON   0x11
#define SUMP_XOFF  0x13
#define SUMP_TRIGGER_MASK 0xC0
#define SUMP_TRIGGER_VALUES 0xC1
#define SUMP_TRIGGER_CONFIG 0xC2
#define SUMP_SET_DIVIDER 0x80
#define SUMP_SET_READ_DELAY_COUNT 0x81
#define SUMP_SET_FLAGS 0x82
#define SUMP_SET_RLE 0x0100
#define SUMP_GET_METADATA 0x04

namespace logic_analyzer {

/// Logic Analzyer Capturing Status
enum Status {ARMED, TRIGGERED, STOPPED};


enum Event {RESET, STATUS, CAPUTRE_SIZE, CAPTURE_FREQUNCY,TRIGGER_VALUES,TRIGGER_MASK, READ_DLEAY_COUNT, FLAGS};
typedef void (*EventHandler)(Event event);

/// Define the datatype for PinBitArray: usually it is a uint8_t, but we could use uint16_t or uint32_t as well.
typedef  PINS_TYPE PinBitArray;


/// provides the logger stream
Stream *logger() {
#ifdef LOG
    return &LOG;
#else
    return nullptr;
#endif
}

/// checks if loggin is active
bool isLoggingActive() {
    return logger() != nullptr;
}

/**
*  Prints the content to the logger output stream
*/
void printLog(const char* fmt, ...) {
#ifdef LOG
    if (isLoggingActive()){
        char serial_printf_buffer[LOG_BUFFER_SIZE] = {0};
        va_list args;
        va_start(args,fmt);
        vsnprintf(serial_printf_buffer,LOG_BUFFER_SIZE, fmt, args);
        logger()->println(serial_printf_buffer);
        va_end(args);
    }
#endif
}

/**
 * @brief 4 Byte SUMP Protocol Command.  The uint8Values data is provided in network format (big endian) while
 * the internal representation is little endian on the 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Sump4ByteComandArg {
    public:
        uint8_t uint8Values[5];
        
        uint16_t get16(int idx) {
            return ntohs(uint16Values[idx]);
        }
        uint16_t get32() {
            return ntohl(uint32Value[0]);
        }

    protected:
        uint16_t* uint16Values = (uint16_t*) &uint8Values[0];
        uint32_t* uint32Value = (uint32_t*) &uint8Values[0];

};


/**
 * @brief Processor specific implementation Logic - abstract class
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PinReaderAbstract {
    public:
        /// reads status of all pins from processor
        virtual PinBitArray readAll() = 0;

};

/**
 * @brief Data is captured in a ring buffer. If the buffer is full we overwrite the oldest entries....
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBuffer {
    public:
        RingBuffer(int size){
            this->size_count = size+1;
            data = new PinBitArray[this->size_count];
            if (data==nullptr){
                printLog("Requested capture size is too big");
            }
        }

         ~RingBuffer(){
             delete[] data;
         }

        /// adds an entry - if there is no more space we overwrite the oldest value
        void write(PinBitArray value){
            if (ignore_count > 0) {
                ignore_count--;
                return;
            }
            data[write_pos] = value;
            write_pos++;
            if (write_pos>size_count){
                write_pos = 0;
            }
            available_count++;
            if (available_count>size_count){
                available_count = size_count;
            }
        }

        /// reads the next available entry from the buffer
        PinBitArray read() {
            PinBitArray result = 0;
            if (available_count>0){
                result = data[read_pos++];
                if (read_pos>size_count){
                    read_pos = 0;
                }
                available_count--;
            }
            return result;
        }

        /// clears all entries
        void clear() {
            available_count = 0;
            write_pos = 0;
            read_pos = 0;
        }

        /// clears n entries from the buffer - if the number is bigger then the available data we ignore some future data
        void clear(int count){
            if (count>available_count){
                ignore_count = count - available_count;
                size_count = available_count;
            }            
            for (int j=0;j<count;j++){
                read();
            }
        }

        /// returns the number of available entries
        int available() {
            return available_count;
        }

        /// returns the max buffer size
        int size() {
            return size_count;
        }

        PinBitArray *data_ptr(){
            return data;
        }

    private:
        int available_count = 0;
        int size_count = 0;
        int write_pos = 0;
        int read_pos = 0;
        int ignore_count = 0;
        PinBitArray *data;
};


/**
 * @brief Main Logic Analyzer API using the SUMP Protocol
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LogicAnalyzer {
    public:
        /// Default Constructor
        LogicAnalyzer() {
            printLog("LogicAnalyzer");
        }

        /// Destructor
        ~LogicAnalyzer() {
            printLog("~LogicAnalyzer");
            if (this->buffer_ptr)  delete this->buffer_ptr;
        }

        /// starts the processing
        void begin(Stream &procesingStream, PinReaderAbstract *impl_ptr, uint32_t maxCaptureSize, int pinStart=0, int numberOfPins=8, bool setup_pins=false){
            printLog("begin");
            this->setStream(procesingStream);
            this->max_capure_size = maxCaptureSize;
            this->read_count = maxCaptureSize;
            this->delay_count = maxCaptureSize;
            this->impl_ptr = impl_ptr;
            this->pin_start = pinStart;
            this->pin_numbers = numberOfPins;
            this->buffer_ptr = new RingBuffer(maxCaptureSize);

            // by default the pins are in read mode - so it is usually not really necesarry to set the mode to input
            if (setup_pins){
                // pinmode imput for requested pins
                for (int j=pinStart;j<numberOfPins;j++){
                    pinMode(pinStart+j, INPUT);
                }
            }

            // set initial status
            setStatus(STOPPED);
    
            // setup LED
            #ifdef LED_BUILTIN
            pinMode(LED_BUILTIN, OUTPUT); 
            #endif

            printLog("begin-end");

        }

        /// provides command output stream of capturing divice
        Stream &stream() {
            return *stream_ptr;
        }

        /// starts the capturing of the data
        virtual void capture(bool is_dump=true) {
            printLog("capture(trigger)");
            // waitring for trigger
            if (trigger_mask) {
                printLog("waiting for trigger");
                while ((trigger_values ^ captureSample()) & trigger_mask)
                    ;
                printLog("triggered");
                setStatus(TRIGGERED);
            } else {
                setStatus(TRIGGERED);
            }

            // remove unnecessary entries from buffer based on delayCount & readCount
            printLog("capture(buffer)");
            long keep = read_count - delay_count;   
            if (keep > 0 && buffer_ptr->available()>keep)  {
                printLog("keeping last %ld entries",keep);
                buffer_ptr->clear(buffer_ptr->available() - keep);
            } else if (keep < 0)  {
                printLog("ignoring first %ld entries",abs(keep));
                buffer_ptr->clear(buffer_ptr->available() + abs(keep));
            } else if (keep==0l){
                printLog("starting with clean buffer");
                buffer_ptr->clear();
            } 

            // Capture
            if (is_continuous_capture){
                printLog("contuinous capturing");
                while(status == TRIGGERED){
                    long end_us = delay_time_us + micros();
                    captureSample();   
                    long delay = end_us - micros();
                    if (delay>0) delayMicroseconds(delay);
                }
            } else {
                printLog("captureing %ld entries",read_count);
                while(status == TRIGGERED && buffer_ptr->available() < read_count ){
                    long end_us = delay_time_us + micros();
                    captureSample();   
                    long delay = end_us - micros();
                    if (delay>0) delayMicroseconds(delay);
                }
                setStatus(STOPPED);
                printLog("capture-done: %lu",buffer_ptr->available());
                if (is_dump) dumpData();
            }

        }

        /// process the next available command - if any
        void processCommand(){
            //printLog("processCommand");
            if (hasCommand()){
                int cmd = command();
                printLog("processCommand %d", cmd);
                processCommand(cmd);
            }
        }

        /// provides the trigger values
        PinBitArray triggerValues() {
            return trigger_values;
        }

        /// defines the trigger values
        void setTriggerValues(PinBitArray values){
            trigger_values = values;
            printLog("--> setTriggerValues: %u", (uint32_t) values);
            raiseEvent(TRIGGER_VALUES);
        } 

        /// provides the trigger mask
        PinBitArray triggerMask() {
            return trigger_mask;
        }

        /// defines the trigger mask
        void setTriggerMask(PinBitArray values){
            trigger_mask = values;
            printLog("--> setTriggerValues: %u", (uint32_t) values);
            raiseEvent(TRIGGER_MASK);
        } 

        /// provides the read count
        int readCount() {
            return read_count;
        }

        /// defines the read count
        void setReadCount(int count){
            read_count = count;
        }

        /// provides the delay count
        int delayCount() {
            return delay_count;
        }

        /// defines the delay count
        void setDelayCount(int count){
            printLog("--> setDelayCount: %d", count);
            delay_count = count;
        }

        /// provides the caputring frequency
        uint64_t captureFrequency() {
            return frequecy_value;
        }

        /// defines the caputring frequency
        void setCaptureFrequency(uint64_t value){
            frequecy_value = value;
            printLog("--> setCaptureFrequency: %lu", frequecy_value);
            delay_time_us = (1000000.0 / value ) - 1;
            printLog("--> delay_time_us: %lu", delay_time_us);
            raiseEvent(CAPTURE_FREQUNCY);
        }

        /// checks if the caputring is continuous
        bool isContinuousCapture(){
            return is_continuous_capture;
        }

        /// defines the caputring as continuous
        void setContinuousCapture(bool cont){
            is_continuous_capture = cont;
        }

        /// defines a event handler that gets notified on some defined events
        void setEventHandler(EventHandler eh){
            eventHandler = eh;
        }

        /// Resets the status and buffer
        void reset(){
            setStatus(STOPPED);
            memset(buffer_ptr->data_ptr(),0x00, max_capure_size*sizeof(PinBitArray));
            buffer_ptr->clear();
            raiseEvent(RESET);
        }


    protected:
        bool is_continuous_capture = false; // => continous capture
        uint32_t max_capure_size;
        int trigger_pos = -1;
        int read_count = 0;
        int delay_count = 0;
        int pin_start = 0;
        int pin_numbers = 0;
        uint64_t frequecy_value = 10000000;  // in 0.001 hz
        uint64_t delay_time_us;
        uint64_t sump_reset_igorne_timeout=0;
        Stream *stream_ptr;
        Status status;
        PinBitArray trigger_mask = 0;
        PinBitArray trigger_values = 0;
        PinReaderAbstract *impl_ptr = nullptr;
        RingBuffer* buffer_ptr = nullptr;
        const char* description = "ARDUINO";
        const char* device_id = "1ALS";
        const char* firmware_version = "\x020.13";
        const char* protocol_version = "\x041\x002";
        Sump4ByteComandArg cmd4;
        EventHandler eventHandler = nullptr;

        /// Defines the command stream to Pulseview capturing divice
        void setStream(Stream &stream){
            stream_ptr = &stream;
        }

        /// Defines the actual status
        void setStatus(Status status){
            this->status = status;
            raiseEvent(STATUS);
            #ifdef LED_BUILTIN
            digitalWrite(LED_BUILTIN, this->status!=STOPPED);   // turn the LED on if not stopped
            #endif
        }

        /// raises an event
        void raiseEvent(Event event){
            if (eventHandler!=nullptr) eventHandler(event);
        }

        /// checks if there is a command available
        bool hasCommand() {
            return stream_ptr->available() > 0;
        }

        /// gets the next 1 byte command
        uint8_t command() {
            int command = stream_ptr->read();
            return command;
        }

        /// gets the next 4 byte command
        Sump4ByteComandArg &commandExt() {
            delay(10);
            stream_ptr->readBytes(cmd4.uint8Values, 4);
            return cmd4;
        }

        /// writes the status of all activated pins to the capturing device
        void write(PinBitArray bits) {
            // write 4 bytes
            stream_ptr->write(htonl(bits));
        }

        /// writes a byte command with uint32_t number argument
        void write(uint8_t cmd, uint32_t number){
            stream().write(cmd);
            uint32_t toSend = htonl(number);
            stream().write((byte*)&toSend,sizeof(uint32_t));
            stream().flush();
        }

        /// writes a byte command with char* argument
        void write(uint8_t cmd, const char* str){
            stream().write(cmd);
            stream().print(str);
            stream().write("\x000",1);
            stream().flush();
        }

        /// captures all pins 
        PinBitArray captureSample() {
            // actual state
            PinBitArray actual = impl_ptr->readAll();

            // buffer single capture cycle
            if (is_continuous_capture) {
                write(actual);
            } else if (status==TRIGGERED) {
                buffer_ptr->write(actual);
            } 
            return actual;          
        }

        /// Provides the result of the 4 byte command
        Sump4ByteComandArg getSump4ByteComandArg() {
            Sump4ByteComandArg result = commandExt();
            return result;
        }

        /// Provides the command as PinBitArray
        PinBitArray commandExtPinBitArray() {
            Sump4ByteComandArg cmd = getSump4ByteComandArg(); 
            switch(sizeof(PinBitArray)) {
                case 1:
                    return (PinBitArray) cmd.uint8Values[0];
                case 2:
                    return (PinBitArray) cmd.get16(0);
                default:
                    return (PinBitArray) cmd.get32();
            }  
        }

        /// dumps the caputred data to the recording device
        void dumpData() {
            printLog("dumpData: %lu",buffer_ptr->available());
            if (buffer_ptr==nullptr || impl_ptr==nullptr) return;
            while(buffer_ptr->available()>0){
                uint32_t value = buffer_ptr->read();
                stream().write((uint8_t*)&value, sizeof(uint32_t));
            }
            stream().flush();
        }

        /**
        * This function calculates what delay we need for the specific sample rate.
        * The dividers are based on SUMP's 100Mhz clock.
        * For example, a 1MHz sample rate has a divider of 99 (0x63 in the command
        * byte).
        * rate = clock / (divider + 1)
        * rate = 100,000,000 / (99 + 1)
        * result is 1,000,000 saying we want a 1MHz sample rate.
        * We calculate our inter sample delay from the divider and the delay between
        * samples gives us the sample rate per second.
        * So for 1MHz, delay = (99 + 1) / 100 which gives us a 1 microsecond delay.
        * For 500KHz, delay = (199 + 1) / 100 which gives us a 2 microsecond delay.
        *
        */
        void setupDelay(unsigned long divider) {
            if (impl_ptr==nullptr) return;
            // calculate frequency
            setCaptureFrequency(100000000l / (divider + 1));
        }

        /**
        * This function returns the metadata about our capabilities.  It is sent in
        * response to the  OpenBench Logic Sniffer extended get metadata command
        *
        */
        void sendMetadata() {
            printLog("sendMetadata");
            if (impl_ptr==nullptr) return;
            write(0x01, description);
            // number of probes 
            write(0x20, pin_numbers);
            // sample memory 
            write(0x21, max_capure_size);
            // sample rate e.g. (4MHz) 
            write(0x23, frequecy_value);
            // protocol version & end
            stream().write(protocol_version, strlen(protocol_version)+1);
            stream().flush();
        }

        /**
         *  Proposess the SUMP commands
         */
        void processCommand(int cmd){
            if (buffer_ptr==nullptr || impl_ptr==nullptr) return;

            switch (cmd) {
                /**
                 * Resets the buffer and processing status. Resets are repeated 5 times!
                 */
                case SUMP_RESET:
                    // debounce reset
                    if (millis()>sump_reset_igorne_timeout){
                        printLog("->SUMP_RESET");
                        sump_reset_igorne_timeout = millis()+ 500;
                    }
                    break;

                /**
                 * Asks for device identification. The device will respond with four bytes. 
                 */
                case SUMP_ID:
                    printLog("->SUMP_ID");
                    this->stream().write(device_id, 4);
                    this->stream().flush();
                    break;

                /*
                * Captures the data
                */
                case SUMP_ARM:
                    printLog("->SUMP_ARM");
                    setStatus(ARMED);
                    capture();
                    break;

                /*
                * the trigger mask byte has a '1' for each enabled trigger so
                * we can just use it directly as our trigger mask.
                */
                case SUMP_TRIGGER_MASK:
                    printLog("->SUMP_TRIGGER_MASK");
                    setTriggerMask(commandExtPinBitArray());
                    break;

                /*
                * trigger_values can be used directly as the value of each bit
                * defines whether we're looking for it to be high or low.
                */
                case SUMP_TRIGGER_VALUES:
                    printLog("->SUMP_TRIGGER_VALUES");
                    setTriggerValues(commandExtPinBitArray());
                    break;


                /* read the rest of the command bytes but ignore them. */
                case SUMP_TRIGGER_CONFIG: 
                    printLog("->SUMP_TRIGGER_CONFIG");
                    getSump4ByteComandArg();                     
                    break;
                /*
                * the shifting needs to be done on the 32bit unsigned long variable
                * so that << 16 doesn't end up as zero.
                */
                case SUMP_SET_DIVIDER: {
                        printLog("->SUMP_SET_DIVIDER");
                        Sump4ByteComandArg cmd = getSump4ByteComandArg();
                        uint32_t divider = cmd.get32();
                        printLog("-divider: %lu\n", divider);
                        setupDelay(divider);
                    }
                    break;

                /*
                * this just sets up how many samples there should be before
                * and after the trigger fires.  The readCount is total samples
                * to return and delayCount number of samples after the trigger.
                * this sets the buffer splits like 0/100, 25/75, 50/50
                * for example if readCount == delayCount then we should
                * return all samples starting from the trigger point.
                * if delayCount < readCount we return (readCount - delayCount) of
                * samples from before the trigger fired.
                */
                case SUMP_SET_READ_DELAY_COUNT: {
                        printLog("->SUMP_SET_READ_DELAY_COUNT");
                        Sump4ByteComandArg cmd = getSump4ByteComandArg();
                        read_count = min((uint32_t)cmd.get16(0), max_capure_size);
                        delay_count = min((uint32_t)cmd.get16(1),max_capure_size);
                        printLog("--> read_count: %d", read_count);
                        printLog("--> delay_count: %d", delay_count);
                        raiseEvent(READ_DLEAY_COUNT);
                    }
                    break;

                /* read the rest of the command bytes and check if RLE is enabled. */
                case SUMP_SET_FLAGS: {
                        printLog("->SUMP_SET_FLAGS");
                        Sump4ByteComandArg cmd =  getSump4ByteComandArg();
                        is_continuous_capture = ((cmd.uint8Values[1] & 0B1000000) != 0);
                        printLog("--> is_continuous_capture: %d\n", is_continuous_capture);
                        raiseEvent(FLAGS);

                    }
                    break;

                /*
                * We return a description of our capabilities.
                * Check the function's comments below.
                */
                case SUMP_GET_METADATA:
                    printLog("->SUMP_GET_METADATA");
                    sendMetadata();
                    break;

                /* ignore any unrecognized bytes. */
                default:
                    printLog("->UNHANDLED command: %d", cmd);
                    break;
                
            };
    }
};

} // namespace


