/**
 * @file logic_analyzer_generic.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @brief Generic implementation of SUMP based logic analyzer
 * 
 */
#pragma once

#include "Arduino.h"
#include "config.h"
#include "network.h"

// Max numbers of logged characters in a line
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

/// forward declarations
class AbstractCapture;
class Capture;
class LogicAnalyzer;


/// Logic Analzyer Capturing Status
enum Status : uint8_t {ARMED, TRIGGERED, STOPPED};

/// Events
enum Event : uint8_t {RESET, STATUS, CAPUTRE_SIZE, CAPTURE_FREQUNCY,TRIGGER_VALUES,TRIGGER_MASK, READ_DLEAY_COUNT, FLAGS};
typedef void (*EventHandler)(Event event);

/// Logger Stream
Stream *logger_ptr = nullptr;

///  Prints the content to the logger output stream
inline void printLog(const char* fmt, ...) {
    if (logger_ptr!=nullptr) {
        char serial_printf_buffer[LOG_BUFFER_SIZE] = {0};
        va_list args;
        va_start(args,fmt);
        vsnprintf(serial_printf_buffer,LOG_BUFFER_SIZE, fmt, args);
        logger_ptr->println(serial_printf_buffer);
        va_end(args);
    }
}

/**
 * @brief 4 Byte SUMP Protocol Command.  The uint8Values data is provided in network format (big endian) while
 * the internal representation is little endian on the 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Sump4ByteComandArg {
    public:
        /// Provides a pointer to the memory
        uint8_t *getPtr() {
            return uint8Values;
        }

        /// Provides a uint16_t value
        uint16_t get16(int idx) {
            return ntohs(uint16Values[idx]);
        }

        /// Provides a uint32_t value
        uint32_t get32() {
            return ntohl(uint32Value[0]);
        }

    protected:
        uint8_t uint8Values[4];
        uint16_t* uint16Values = (uint16_t*) &uint8Values[0];
        uint32_t* uint32Value = (uint32_t*) &uint8Values[0];

};

/**
 * @brief Data is captured in a ring buffer. If the buffer is full we overwrite the oldest entries....
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBuffer {
    public:
        RingBuffer(size_t size){
            this->size_count = size;
            data = new PinBitArray[this->size_count];
            if (data==nullptr){
                printLog("Requested capture size is too big");
                this->size_count = 0;
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
            data[write_pos++] = value;
            if (write_pos>size_count){
                write_pos = 0;
            }
            if (available_count<size_count){
                available_count++;
            } else {
                read_pos = write_pos+1;
            }
        }

        /// reads the next available entry from the buffer
        PinBitArray read() {
            PinBitArray result = 0;
            if (available_count>0){
                if (read_pos>size_count){
                    read_pos = 0;
                }
                result = data[read_pos++];
                available_count--;
            }
            return result;
        }

        /// clears all entries
        void clear() {
            ignore_count = 0;
            available_count = 0;
            write_pos = 0;
            read_pos = 0;
        }

        /// clears n entries from the buffer - if the number is bigger then the available data we ignore some future data
        void clear(size_t count){
            ignore_count = 0;
            if (count>available_count){
                // calculate number of future entries to ignore
                ignore_count = count - available_count;
            } 
            // remove count entries            
            for (int j=0;j<count && available()>0;j++){
                read();
            }
        }

        /// returns the number of available entries
        size_t available() {
            return available_count;
        }

        /// returns the max buffer size
        size_t size() {
            return size_count;
        }

        PinBitArray *data_ptr(){
            return data;
        }

    private:
        size_t available_count = 0;
        size_t size_count = 0;
        size_t write_pos = 0;
        size_t read_pos = 0;
        size_t ignore_count = 0;
        PinBitArray *data;
};

/**
 * @brief Common State information for the Logic Analyzer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class LogicAnalyzerState {
    public:
        friend class AbstractCapture;
        friend class LogicAnalyzer;
        friend class Capture;

    protected:
        volatile Status status_value;
        bool is_continuous_capture = false; // => continous capture
        uint32_t max_capture_size = 1000;
        int trigger_pos = -1;
        int read_count = 0;
        int delay_count = 0;
        int pin_start = 0;
        int pin_numbers = 0;
        uint64_t frequecy_value;  // in hz
        uint64_t max_frequecy_value;  // in hz
        uint64_t max_frequecy_threshold;  // in hz
        uint64_t delay_time_us;
        PinBitArray trigger_mask = 0;
        PinBitArray trigger_values = 0;
        Sump4ByteComandArg cmd4;
        Stream *stream_ptr;
        RingBuffer *buffer_ptr = nullptr;;
        EventHandler eventHandler = nullptr;

        /// Defines the actual status
        void setStatus(Status status){
            status_value = status;
            raiseEvent(STATUS);
        }

        /// raises an event
        void raiseEvent(Event event){
            if (eventHandler!=nullptr) eventHandler(event);
        }
};       

/**
 * @brief Abstract Class for Capturing Logic. Create your own subclass if you want to implement your own
 * optimized capturing logic. Otherwise just use the provided Capture class.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AbstractCapture {
    public:
        /// Default Constructor
        AbstractCapture(){
        }

        /// Destructor
        ~AbstractCapture(){
            if (this->pin_reader_ptr!=nullptr) {
                delete this->pin_reader_ptr;
            }
        }

        /// Assigns the state
        void setState(LogicAnalyzerState &state) {
            this->state_ptr = &state;
            this->buffer_ptr = state.buffer_ptr;
            if (state.buffer_ptr==nullptr){
                printLog("The buffer is not available! - did you call LogicAnalyzer.begin() ?");
                state.buffer_ptr = new RingBuffer(state.max_capture_size);
            }
            if (state.stream_ptr==nullptr){
                printLog("The command stream is not available! - did you call LogicAnalyzer.begin() ?");
                state.stream_ptr = &Serial;

            }
            if (this->pin_reader_ptr==nullptr){
                this->pin_reader_ptr = new PinReader(state.pin_start);
            }
        }

        /// Captures the data and dumps the result
        virtual void capture() = 0;

        /// Provides the State
        LogicAnalyzerState &state() {
            return *state_ptr;
        }

        /// Provides access to the SUMP command stream
        Stream &stream() {
            return *(state_ptr->stream_ptr);
        }

        /// Provides access to the PinReader
        PinReader &pinReader() {
            return *pin_reader_ptr;
        }

        /// Provides direct access to the ring buffer
        RingBuffer &buffer(){
            return *buffer_ptr;
        }

    protected:
        LogicAnalyzerState *state_ptr;
        RingBuffer *buffer_ptr = nullptr;
        PinReader *pin_reader_ptr;

        /// writes the status of all activated pins to the capturing device
        void write(PinBitArray bits) {
            // write 4 bytes
            state().stream_ptr->write(htonl(bits));
        }

        /// Defines the actual status
        void setStatus(Status status){
            state().setStatus(status);
        }

        /// dumps the caputred data to the recording device
        void dumpData() {
            printLog("dumpData: %lu",buffer_ptr->available());
            if (buffer_ptr==nullptr) return;
            while(buffer_ptr->available()>0){
                uint32_t value = buffer_ptr->read();
                stream().write((uint8_t*)&value, sizeof(uint32_t));
            }
            stream().flush();
        }
       
};

/**
 * @brief Default Implementation for the Capturing Logic. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Capture : public AbstractCapture {
    public:
        /// Default Constructor
        Capture() : AbstractCapture(){
        }

        /// starts the capturing of the data
        virtual void capture(){
            // if frequecy_value >= max_frequecy_value -> capture at max speed
            capture(state().frequecy_value >= state().max_frequecy_threshold); 
        }

        /// Generic Capturing of requested number of examples into the buffer
        void captureAll() {
            printLog("captureing %ld entries", state_ptr->read_count);
            while(state_ptr->status_value == TRIGGERED && buffer_ptr->available() < state_ptr->read_count ){
                captureSampleFast();   
                delayMicroseconds(state_ptr->delay_time_us);
            }
        }

        /// Capturing of requested number of examples into the buffer at maximum speed 
        void captureAllMaxSpeed() {
            printLog("captureing %ld entries",state_ptr->read_count);
            while(state_ptr->status_value == TRIGGERED && buffer_ptr->available() < state_ptr->read_count ){
                captureSampleFast();
            }
        }

        /// Continuous capturing at the requested speed
        void captureAllContinous() {
            printLog("contuinous capturing");
            while(state_ptr->status_value == TRIGGERED){
                captureSampleFastContinuous();   
                delayMicroseconds(state_ptr->delay_time_us);
            }
        }

        /// Continuous capturing at max speed
        void captureAllContinousMaxSpeed() {
            printLog("contuinous capturing with max speed");
            while(state_ptr->status_value == TRIGGERED){
                captureSampleFastContinuous();   
            }
        }

        /// captures one singe entry for all pins and writes it to the buffer
        void captureSampleFast() {
            buffer_ptr->write(pinReader().readAll());            
        }

        /// captures one singe entry for all pins and writes it to output stream
        void captureSampleFastContinuous() {
            write(pinReader().readAll());            
        }

        /// captures one single entry for all pins and provides the result - used by the trigger
        PinBitArray captureSample() {
            // actual state
            PinBitArray actual = pinReader().readAll();

            // buffer single capture cycle
            if (state_ptr->is_continuous_capture) {
                write(actual);
            } else if (state_ptr->status_value==TRIGGERED) {
                buffer_ptr->write(actual);
            } 
            return actual;          
        }

    protected:
        /// starts the capturing of the data
        void capture(bool is_max_speed) {
            printLog("capture(trigger)");
            // waiting for trigger
            if (state_ptr->trigger_mask) {
                printLog("waiting for trigger");
                while ((state_ptr->trigger_values ^ captureSample()) & state_ptr->trigger_mask)
                    ;
                printLog("triggered");
                setStatus(TRIGGERED);
            } else {
                setStatus(TRIGGERED);
            }

            // remove unnecessary entries from buffer based on delayCount & readCount
            printLog("capture(buffer)");
            long keep = state_ptr->read_count - state_ptr->delay_count;   
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

            // Start Capture
            if (is_max_speed){
                if (state_ptr->is_continuous_capture){
                    captureAllContinousMaxSpeed();
                } else {
                    captureAllMaxSpeed();
                    setStatus(STOPPED);
                    printLog("capture-done: %lu",buffer_ptr->available());
                    dumpData();
                }
            } else { 
                if (state_ptr->is_continuous_capture){
                    captureAllContinous();
                } else {
                    captureAll();
                    setStatus(STOPPED);
                    printLog("capture-done: %lu",buffer_ptr->available());
                    dumpData();
                }
            }
        }


};

/**
 * @brief Main Logic Analyzer API using the SUMP Protocol.
 * When you try to connect to the Logic Analzyer - SUMP calls the following requests
 * 1) RESET, 2) ID and then 3) GET_METADATA: This is used to populate the Device!
 * All other requests are called when you click on the capture button.
 * 
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
            if (la_state.buffer_ptr)  {
                delete la_state.buffer_ptr;
                la_state.buffer_ptr = nullptr;
            }
        }

        /**
         * @brief Starts the processing
         * 
         * @param procesingStream Stream which is used to communicate to pulsview
         * @param capture  AbstractCapture
         * @param maxCaptureFreq Max Supported Capturing Frequency
         * @param maxCaptureFreqThreshold Threshold which is used to change to the 'full speed' implementation which does not dontain any delays
         * @param maxCaptureSize Maximum number of captured entries
         * @param pinStart Start GPIO Pin Number for capturing
         * @param numberOfPins Number of subsequent pins to capture
         * @param setup_pins Change the pin mode to input 
         */
        void begin(Stream &procesingStream, AbstractCapture *capture, uint32_t maxCaptureFreq, uint32_t maxCaptureFreqThreshold,  uint32_t maxCaptureSize, uint8_t pinStart=0, uint8_t numberOfPins=8, bool setup_pins=false){
            printLog("begin");
            setStream(procesingStream);
            this->capture_ptr = capture;

            la_state.max_frequecy_value = maxCaptureFreq;
            la_state.max_frequecy_threshold = maxCaptureFreqThreshold;
            la_state.max_capture_size = maxCaptureSize;
            la_state.read_count = maxCaptureSize;
            la_state.delay_count = maxCaptureSize;
            la_state.pin_start = pinStart;
            la_state.pin_numbers = numberOfPins;
            la_state.buffer_ptr = new RingBuffer(maxCaptureSize);

            // assign state to capture 
            if (capture!=nullptr) {
                capture->setState(la_state);
            }

            // by default the pins are in read mode - so it is usually not really necesarry to set the mode to input
            if (setup_pins){
                // pinmode imput for requested pins
                for (int j=pinStart;j<numberOfPins;j++){
                    pinMode(pinStart+j, INPUT);
                }
            }

            // set initial status
            setStatus(STOPPED);
    
            printLog("begin-end");

        }

        /// provides command output stream of capturing divice
        Stream &stream() {
            return *(la_state.stream_ptr);
        }

        Status status() {
            return la_state.status_value;
        }

        /// Defines the actual status
        void setStatus(Status status){
            la_state.setStatus(status);
        }

        /// process the next available command - if any
        void processCommand(){
            if (hasCommand()){
                int cmd = command();
                printLog("processCommand %d", cmd);
                processCommand(cmd);
            }
        }

        /// provides the trigger values
        PinBitArray triggerValues() {
            return la_state.trigger_values;
        }

        /// defines the trigger values
        void setTriggerValues(PinBitArray values){
            la_state.trigger_values = values;
            printLog("--> setTriggerValues: %u", (uint32_t) values);
            raiseEvent(TRIGGER_VALUES);
        } 

        /// provides the trigger mask
        PinBitArray triggerMask() {
            return la_state.trigger_mask;
        }

        /// defines the trigger mask
        void setTriggerMask(PinBitArray values){
            la_state.trigger_mask = values;
            printLog("--> setTriggerValues: %u", (uint32_t) values);
            raiseEvent(TRIGGER_MASK);
        } 

        /// provides the read count
        int readCount() {
            return la_state.read_count;
        }

        /// defines the read count
        void setReadCount(int count){
            la_state.read_count = count;
        }

        /// provides the delay count
        int delayCount() {
            return la_state.delay_count;
        }

        /// defines the delay count
        void setDelayCount(int count){
            printLog("--> setDelayCount: %d", count);
            la_state.delay_count = count;
        }

        /// provides the caputring frequency
        uint64_t captureFrequency() {
            return la_state.frequecy_value;
        }

        /// Provides the delay time between measurements in microseconds 
        uint64_t delayTimeUs() {
            return la_state.delay_time_us;
        }

        /// defines the caputring frequency
        void setCaptureFrequency(uint64_t value){
            la_state.frequecy_value = value;
            printLog("--> setCaptureFrequency: %lu", la_state.frequecy_value);
            la_state.delay_time_us = (1000000.0 / value ) - 1;
            printLog("--> delay_time_us: %lu", la_state.delay_time_us);
            raiseEvent(CAPTURE_FREQUNCY);
        }

        /// checks if the caputring is continuous
        bool isContinuousCapture(){
            return la_state.is_continuous_capture;
        }

        /// defines the caputring as continuous
        void setContinuousCapture(bool cont){
            la_state.is_continuous_capture = cont;
        }

        /// defines a event handler that gets notified on some defined events
        void setEventHandler(EventHandler eh){
            la_state.eventHandler = eh;
        }

        /// Resets the status and buffer
        void reset(){
            setStatus(STOPPED);
            memset(la_state.buffer_ptr->data_ptr(),0x00, la_state.max_capture_size*sizeof(PinBitArray));
            la_state.buffer_ptr->clear();
            raiseEvent(RESET);
        }

        /// returns the max buffer size
        size_t size() {
            return la_state.buffer_ptr->size();
        }

        /// returns the avialable buffer entries
        size_t available() {
            return la_state.buffer_ptr->available();
        }

        /// Defines the logging Stream
        void setLogger(Stream &logger){
            logger_ptr = &logger;
        }

        /// Switch automatic capturing on ARMED status on/off 
        void setCaptureOnArm(bool capture){
            is_capture_on_arm = capture;
        }

        /// Provides a reference to the LogicAnalyzerState
        LogicAnalyzerState &state() {
            return la_state;
        }

    protected:
        bool is_capture_on_arm = true;
        uint64_t sump_reset_igorne_timeout=0;
        AbstractCapture *capture_ptr = nullptr;
        LogicAnalyzerState la_state;
        const char* description = "ARDUINO";
        const char* device_id = "1ALS";
        const char* firmware_version = "\x020.13";
        const char* protocol_version = "\x041\x002";

        /// Defines the command stream to Pulseview capturing divice
        void setStream(Stream &stream){
            la_state.stream_ptr = &stream;
        }

        /// checks if there is a command available
        bool hasCommand() {
            return la_state.stream_ptr->available() > 0;
        }

        /// gets the next 1 byte command
        uint8_t command() {
            int command = stream().read();
            return command;
        }

        /// gets the next 4 byte command
        Sump4ByteComandArg &commandExt() {
            delay(10);
            la_state.stream_ptr->readBytes(la_state.cmd4.getPtr(), 4);
            return la_state.cmd4;
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
                    return (PinBitArray) cmd.getPtr()[0];
                case 2:
                    return (PinBitArray) cmd.get16(0);
                default:
                    return (PinBitArray) cmd.get32();
            }  
        }

        /// raises an event
        void raiseEvent(Event event){
            la_state.raiseEvent(event);
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
            write(0x01, description);
            // number of probes 
            write(0x20, la_state.pin_numbers);
            // sample memory 
            write(0x21, la_state.max_capture_size);
            // sample rate e.g. (4MHz) 
            write(0x23, la_state.frequecy_value);
            // protocol version & end
            stream().write(protocol_version, strlen(protocol_version)+1);
            stream().flush();
        }

        /**
         *  Proposess the SUMP commands
         */
        void processCommand(int cmd){

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
                * We return a description of our capabilities.
                * Check the function's comments below.
                */
                case SUMP_GET_METADATA:
                    printLog("->SUMP_GET_METADATA");
                    sendMetadata();
                    break;

                /*
                * Captures the data
                */
                case SUMP_ARM:
                    printLog("->SUMP_ARM");
                    setStatus(ARMED);
                    if (capture_ptr!=nullptr && is_capture_on_arm){
                        capture_ptr->capture(); 
                    }
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
                        la_state.read_count = min((uint32_t)cmd.get16(0), la_state.max_capture_size);
                        la_state.delay_count = min((uint32_t)cmd.get16(1),la_state.max_capture_size);
                        printLog("--> read_count: %d", la_state.read_count);
                        printLog("--> delay_count: %d", la_state.delay_count);
                        raiseEvent(READ_DLEAY_COUNT);
                    }
                    break;

                /* read the rest of the command bytes and check if RLE is enabled. */
                case SUMP_SET_FLAGS: {
                        printLog("->SUMP_SET_FLAGS");
                        Sump4ByteComandArg cmd =  getSump4ByteComandArg();
                        la_state.is_continuous_capture = ((cmd.getPtr()[1] & 0B1000000) != 0);
                        printLog("--> is_continuous_capture: %d\n", la_state.is_continuous_capture);
                        raiseEvent(FLAGS);

                    }
                    break;

                /* ignore any unrecognized bytes. */
                default:
                    printLog("->UNHANDLED command: %d", cmd);
                    break;
                
            };
    }
};

} // namespace


