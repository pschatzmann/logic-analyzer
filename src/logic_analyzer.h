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
#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE 80
#endif 

// Max size of buffered print
#ifndef DUMP_RECORD_SIZE
#define DUMP_RECORD_SIZE 1024*1
#endif

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
class RingBuffer;


/// Logic Analzyer Capturing Status
enum Status : uint8_t {STOPPED, ARMED, TRIGGERED};

/// Events
enum Event : uint8_t {RESET, STATUS, CAPUTRE_SIZE, CAPTURE_FREQUNCY,TRIGGER_VALUES,TRIGGER_MASK, READ_DLEAY_COUNT, FLAGS};
typedef void (*EventHandler)(Event event);

PinReader *pin_reader_ptr = nullptr;
/// common access to buffer
RingBuffer *buffer_ptr = nullptr;;
/// Logger Stream
Stream *logger_ptr = nullptr;
/// Command stream
Stream *stream_ptr = nullptr;

/// writes the status of all activated pins to the capturing device
void write(PinBitArray bits) {
    // write 4 bytes
    stream_ptr->write(htonl(bits));
}

// writes a buffer of uint32_t values
void write(uint32_t *buff, size_t n_samples) {
    int written = 0;
    int open = n_samples * sizeof(uint32_t);
    while(open > 0){
        size_t result = stream_ptr->write((const char*)buff + written, open);
        written += result;
        open -= result;
    }
}

// writes a buffer of PinBitArray
void write(PinBitArray *buff, size_t n_samples) {
    // convert to uint32_t
    uint32_t tmp[DUMP_RECORD_SIZE];
    int idx = 0;
    for (int j=0;j<n_samples;j++){
        tmp[idx++] = htonl(buff[j]);
        if (idx==DUMP_RECORD_SIZE){
            write(tmp, idx);
            idx = 0;
        }
    }
    if (idx>0){
        write(tmp, idx);
    }
}


///  Prints the content to the logger output stream
inline void log(const char* fmt, ...) {
    if (logger_ptr!=nullptr) {
        char serial_printf_buffer[LOG_BUFFER_SIZE] = {0};
        va_list args;
        va_start(args,fmt);
        vsnprintf(serial_printf_buffer,LOG_BUFFER_SIZE, fmt, args);
        logger_ptr->println(serial_printf_buffer);
        va_end(args);
        logger_ptr->flush();
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
            return uint16Values[idx];
        }

        /// Provides a uint32_t value
        uint32_t get32() {
            return uint32Value[0];
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
            this->size_count = size + 10;
            data = new PinBitArray[this->size_count];
            if (data==nullptr){
                log("Requested capture size is too big");
                this->size_count = 0;
            }
        }

         ~RingBuffer(){
             if (data!=nullptr){
                delete[] data;
             }
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

        /// 1 SUMP record has 4 bytes - We privide the requested number of buffered values in the output format
        size_t readBuffer(uint32_t *result, size_t read_len){
            size_t result_len;
            for (size_t j=0; j<read_len; j++){
                PinBitArray *ptr = (PinBitArray *) (result+j);
                for (int i=0;i < (4/sizeof(PinBitArray)); i++){
                    ptr[i] = read();
                    if (available()==0){
                        return j+1;
                    }
                } 
            }
            return read_len;
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

        /// Usualy you must not use this function. However for the RP PIO it is quite usefull to indicated that the buffer has been filled 
        void setAvailable(size_t avail){
            available_count = avail;
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
 * @brief Common State information for the Logic Analyzer - provides event handling on State change.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class LogicAnalyzerState {
    public:
        friend class AbstractCapture;
        friend class LogicAnalyzer;
        friend class Capture;

        /// Defines the actual status
        void setStatus(Status status){
            log("setStatus %d", status);
            status_value = status;
            raiseEvent(STATUS);
        }

        Stream &stream() {
            return *stream_ptr;
        }

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
        uint64_t delay_time_us;
        PinBitArray trigger_mask = 0;
        PinBitArray trigger_values = 0;
        Sump4ByteComandArg cmd4;
        EventHandler eventHandler = nullptr;


        /// raises an event
        void raiseEvent(Event event){
            if (eventHandler!=nullptr) eventHandler(event);
        }
} la_state;       



/**
 * @brief Abstract Class for Capturing Logic. Create your own subclass if you want to implement your own
 * optimized capturing logic. Otherwise just use the provided Capture class.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AbstractCapture {
    public:
         friend class LogicAnalyzer;

        /// Default Constructor
        AbstractCapture(){
        }

        /// Destructor
        ~AbstractCapture(){
        }

        /// Provides access to the LogicAnalyzer
        LogicAnalyzer &logicAnalyzer() {
            return *logic_analyzer_ptr;
        }

        /// Provides access to the PinReader
        virtual PinReader &pinReader() {
            return *(pin_reader_ptr);
        }

        /// Sets the status
        virtual void setStatus(Status status){
            la_state.setStatus(status);
        }

        /// Captures the data and dumps the result
        virtual void capture() = 0;

        /// Used to masure the speed - capture into memory w/o dump!
        virtual void captureAll() = 0;


    protected:
        LogicAnalyzer *logic_analyzer_ptr = nullptr;

        virtual void setLogicAnalyzer(LogicAnalyzer &la){
            logic_analyzer_ptr = &la;
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
        Capture(uint64_t maxCaptureFreq, uint64_t maxCaptureFreqThreshold  ) : AbstractCapture(){
            max_frequecy_value = maxCaptureFreq;
            max_frequecy_threshold = maxCaptureFreqThreshold;
        }

        /// starts the capturing of the data
        virtual void capture(){
            log("capture");
            // no capture if request is well above max rate
            if (la_state.frequecy_value > max_frequecy_value + (max_frequecy_value/2)){
                setStatus(STOPPED);
                // Send some dummy data to stop pulseview
                write(0);
                log("The frequency %u is not supported!", la_state.frequecy_value );
                return;
            }

            // if frequecy_value >= max_frequecy_value -> capture at max speed
            capture(la_state.frequecy_value >= max_frequecy_threshold); 
            log("capture-end");
        }

        /// Generic Capturing of requested number of examples into the buffer
        void captureAll() {
            log("captureAll %ld entries", la_state.read_count);
            unsigned long delay_time_us = la_state.delay_time_us;
            while(la_state.status_value == TRIGGERED && buffer_ptr->available() < la_state.read_count ){
                captureSampleFast();   
                delayMicroseconds(delay_time_us);
            }
        }

        /// Capturing of requested number of examples into the buffer at maximum speed 
        void captureAllMaxSpeed() {
            log("captureAllMaxSpeed %ld entries",la_state.read_count);
            while(la_state.status_value == TRIGGERED && buffer_ptr->available() < la_state.read_count ){
                captureSampleFast();
            }
        }

        /// Continuous capturing at the requested speed
        void captureAllContinous() {
            log("captureAllContinous");
            unsigned long delay_time_us = la_state.delay_time_us;
            while(la_state.status_value == TRIGGERED){
                captureSampleFastContinuous();   
                delayMicroseconds(delay_time_us);
            }
        }

        /// Continuous capturing at max speed
        void captureAllContinousMaxSpeed() {
            log("captureAllContinousMaxSpeed");
            while(la_state.status_value == TRIGGERED){
                captureSampleFastContinuous();   
            }
        }

        /// captures one singe entry for all pins and writes it to the buffer
        void captureSampleFast() {
            buffer_ptr->write(pin_reader_ptr->readAll());            
        }

        /// captures one singe entry for all pins and writes it to output stream
        void captureSampleFastContinuous() {
            write(pin_reader_ptr->readAll());            
        }

        /// captures one single entry for all pins and provides the result - used by the trigger
        PinBitArray captureSample() {
            // actual state
            PinBitArray actual = pin_reader_ptr->readAll();

            // buffer single capture cycle
            if (la_state.is_continuous_capture) {
                write(actual);
            } else if (la_state.status_value==TRIGGERED) {
                buffer_ptr->write(actual);
            } 
            return actual;          
        }


    protected:
        uint64_t max_frequecy_value;  // in hz
        uint64_t max_frequecy_threshold;  // in hz

        /// starts the capturing of the data
        void capture(bool is_max_speed) {
            log("capture is_max_speed: %s", is_max_speed ? "true":"false");
            // waiting for trigger
            if (la_state.trigger_mask) {
                log("waiting for trigger");
                while ((la_state.trigger_values ^ captureSample()) & la_state.trigger_mask)
                    ;
            } 
            la_state.setStatus(TRIGGERED);
            log("triggered");

            // remove unnecessary entries from buffer based on delayCount & readCount
            long keep = la_state.read_count - la_state.delay_count;   
            if (keep > 0 && buffer_ptr->available()>keep)  {
                log("keeping last %ld entries",keep);
                buffer_ptr->clear(buffer_ptr->available() - keep);
            } else if (keep < 0)  {
                log("ignoring first %ld entries",abs(keep));
                buffer_ptr->clear(buffer_ptr->available() + abs(keep));
            } else if (keep==0l){
                log("starting with clean buffer");
                buffer_ptr->clear();
            } 

            // Start Capture
            if (is_max_speed){
                if (la_state.is_continuous_capture){
                    captureAllContinousMaxSpeed();
                } else {
                    captureAllMaxSpeed();
                    dumpData();
                    log("capture-done: %lu",buffer_ptr->available());
                    setStatus(STOPPED);
                }
            } else { 
                if (la_state.is_continuous_capture){
                    captureAllContinous();
                } else {
                    captureAll();
                    dumpData();
                    log("capture-done: %lu",buffer_ptr->available());
                    setStatus(STOPPED);
                }
            }
        }

        /// Provides access to the SUMP command stream
        virtual Stream &stream() {
            return *stream_ptr;
        }

       
        /// dumps the caputred data to the recording device
        void dumpData() {
            log("dumpData: %lu",buffer_ptr->available());
            // buffered write 
            uint32_t tmp[DUMP_RECORD_SIZE];
            stream_ptr->setTimeout(10000);
            while(buffer_ptr->available()){
                size_t len = buffer_ptr->readBuffer(tmp, DUMP_RECORD_SIZE);
                write(tmp, len);
            }
            // flush final records - for backward compatibility 
            stream_ptr->flush();
            log("dumpData-end");
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
            log("LogicAnalyzer");
        }

        /// Destructor
        ~LogicAnalyzer() {
            log("~LogicAnalyzer");
            if (buffer_ptr!=nullptr)  {
                delete buffer_ptr;
                buffer_ptr = nullptr;
            }
            if (pin_reader_ptr!=nullptr){
                delete pin_reader_ptr;
                pin_reader_ptr = nullptr;
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
        void begin(Stream &procesingStream, AbstractCapture *capture, uint32_t maxCaptureSize, uint8_t pinStart=0, uint8_t numberOfPins=8, bool setup_pins=false){
            log("begin");
            stream_ptr = &procesingStream;
            this->capture_ptr = capture;

            la_state.max_capture_size = maxCaptureSize;
            la_state.read_count = maxCaptureSize;
            la_state.delay_count = maxCaptureSize;
            la_state.pin_start = pinStart;
            la_state.pin_numbers = numberOfPins;

            // set initial status
            setStatus(STOPPED);

            if (pin_reader_ptr==nullptr){
                pin_reader_ptr = new PinReader(pinStart);
            }

            if (do_allocate_buffer && buffer_ptr==nullptr) {
                buffer_ptr = new RingBuffer(maxCaptureSize);
            }

            // assign state to capture 
            if (capture!=nullptr) {
                capture->setLogicAnalyzer(*this);
            }

            // by default the pins are in read mode - so it is usually not really necesarry to set the mode to input
            if (setup_pins){
                // pinmode imput for requested pins
                for (int j=pinStart;j<numberOfPins;j++){
                    pinMode(pinStart+j, INPUT);
                }
            }
    
            log("begin-end");
        }

        /// Provides the GPIO number of the start pin which is used for capturing
        uint16_t startPin() {
            return la_state.pin_start;
        }

        /// Provides the number of subsequent GPIO pins which will be used for capturing
        uint16_t numberOfPins() {
            return la_state.pin_numbers;
        }

        /// provides command output stream of capturing divice
        Stream &stream() {
            return *(stream_ptr);
        }

        /// provides the actual Status
        Status status() {
            return la_state.status_value;
        }

        /// Defines the actual status
        void setStatus(Status status){
            la_state.setStatus(status);
        }

        /// Provides access to the buffer
        RingBuffer &buffer() {
            return *(buffer_ptr);
        }

        /// process the next available command - Call this function from your Arduino loop()!
        void processCommand(){
            if (hasCommand()){
                int cmd = command();
                log("processCommand %d", cmd);
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
            log("--> setTriggerValues: %u", (uint32_t) values);
            raiseEvent(TRIGGER_VALUES);
        } 

        /// provides the trigger mask
        PinBitArray triggerMask() {
            return la_state.trigger_mask;
        }

        /// defines the trigger mask
        void setTriggerMask(PinBitArray values){
            la_state.trigger_mask = values;
            log("--> setTriggerValues: %u", (uint32_t) values);
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
            log("--> setDelayCount: %d", count);
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
            log("--> setCaptureFrequency: %lu", la_state.frequecy_value);
            la_state.delay_time_us = (1000000.0 / value );
            log("--> delay_time_us: %lu", la_state.delay_time_us);
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
        void clear(){
            log("clear");
            setStatus(STOPPED);
            if (buffer_ptr!=nullptr){
                memset(buffer_ptr->data_ptr(),0x00, la_state.max_capture_size*sizeof(PinBitArray));
                buffer_ptr->clear();
            }
        }

        /// returns the max buffer size
        size_t size() {
            return buffer_ptr == nullptr ? 0 : buffer_ptr->size();
        }

        /// returns the avialable buffer entries
        size_t available() {
            return buffer_ptr == nullptr ? 0 : buffer_ptr->available();
        }

        /// Activate the logging by defining the logging output Stream
        void setLogger(Stream &logger){
            logger_ptr = &logger;
        }

        /// Switch automatic capturing on ARMED status on/off 
        void setCaptureOnArm(bool capture){
            is_capture_on_arm = capture;
        }


        /// Defines the Description
        void setDescription(const char* name) {
            description = name;
        }

        /// Allows to switch of the automatic buffer allocation - call before begin!
        void setAllocateBuffer(bool do_allocate){
            do_allocate_buffer = do_allocate;
        }

        /// starts the capturing
        void capture() {
            if (capture_ptr!=nullptr)
                capture_ptr->capture();
        }


    protected:
        bool is_capture_on_arm = true;
        bool do_allocate_buffer = true;
        uint64_t sump_reset_igorne_timeout=0;
        AbstractCapture *capture_ptr = nullptr;
        const char* description = "ARDUINO";
        const char* device_id = "1ALS";
        const char* firmware_version = "01.0";
        const char* protocol_version = "\x041\x002";

        /// Provides a reference to the LogicAnalyzerState
        LogicAnalyzerState &state() {
            return la_state;
        }


        /// checks if there is a command available
        bool hasCommand() {
            return stream_ptr->available() > 0;
        }

        /// gets the next 1 byte command
        uint8_t command() {
            int command = stream().read();
            return command;
        }

        /// gets the next 4 byte command
        Sump4ByteComandArg &commandExt() {
            delay(10);
            stream().readBytes(la_state.cmd4.getPtr(), 4);
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

        /// Provides the command as PinBitArray
        PinBitArray commandExtPinBitArray() {
            Sump4ByteComandArg cmd = commandExt(); 
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
        void setupDelay(uint32_t divider) {
            // calculate frequency
            setCaptureFrequency(100000000UL / (divider+1));
        }

        /**
        * This function returns the metadata about our capabilities.  It is sent in
        * response to the  OpenBench Logic Sniffer extended get metadata command
        *
        */
        void sendMetadata() {
            log("sendMetadata");
            write(0x01, description);
            write(0x02, firmware_version);
            // number of probes 
            write(0x20, la_state.pin_numbers);
            // sample memory 
            write(0x21, la_state.max_capture_size);
            // sample rate - We do not provide the real max sample rate since this does not have any impact on the gui and provides wrong results!
            //write(0x23, la_state.max_frequecy_value);
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
                        log("=>SUMP_RESET");
                        setStatus(STOPPED);
                        clear();
                        sump_reset_igorne_timeout = millis()+ 500;
                        raiseEvent(RESET);
                    }
                    break;

                /**
                 * Asks for device identification. The device will respond with four bytes. 
                 */
                case SUMP_ID:
                    log("=>SUMP_ID");
                    stream().write(device_id, 4);
                    stream().flush();
                    break;

                /*
                * We return a description of our capabilities.
                * Check the function's comments below.
                */
                case SUMP_GET_METADATA:
                    log("=>SUMP_GET_METADATA");
                    sendMetadata();
                    break;

                /*
                * Captures the data
                */
                case SUMP_ARM:
                    log("=>SUMP_ARM");
                    // clear current data
                    clear();
                    setStatus(ARMED);
                    if (is_capture_on_arm){
                        capture(); 
                    }
                    break;

                /*
                * the trigger mask byte has a '1' for each enabled trigger so
                * we can just use it directly as our trigger mask.
                */
                case SUMP_TRIGGER_MASK:
                    log("=>SUMP_TRIGGER_MASK");
                    setTriggerMask(commandExtPinBitArray());
                    break;

                /*
                * trigger_values can be used directly as the value of each bit
                * defines whether we're looking for it to be high or low.
                */
                case SUMP_TRIGGER_VALUES:
                    log("=>SUMP_TRIGGER_VALUES");
                    setTriggerValues(commandExtPinBitArray());
                    break;


                /* read the rest of the command bytes but ignore them. */
                case SUMP_TRIGGER_CONFIG: 
                    log("=>SUMP_TRIGGER_CONFIG");
                    commandExt();                     
                    break;

                /*
                * the shifting needs to be done on the 32bit unsigned long variable
                * so that << 16 doesn't end up as zero.
                */
                case SUMP_SET_DIVIDER: {
                        log("=>SUMP_SET_DIVIDER");
                        Sump4ByteComandArg cmd = commandExt();
                        uint32_t divider = cmd.get32();
                        log("-divider: %lu\n", divider);
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
                        Sump4ByteComandArg cmd = commandExt();
                        log("=>SUMP_SET_READ_DELAY_COUNT %02X %02X",cmd.get16(0),cmd.get16(1));
                        la_state.read_count = (cmd.get16(0)+1) * 4;
                        la_state.delay_count = (cmd.get16(1)+1) * 4;
                        log("--> read_count: %d", la_state.read_count);
                        log("--> delay_count: %d", la_state.delay_count);
                        raiseEvent(READ_DLEAY_COUNT);
                    }
                    break;

                /* read the rest of the command bytes and check if RLE is enabled. */
                case SUMP_SET_FLAGS: {
                        log("=>SUMP_SET_FLAGS");
                        Sump4ByteComandArg cmd =  commandExt();
                        la_state.is_continuous_capture = ((cmd.getPtr()[1] & 0B1000000) != 0);
                        log("--> is_continuous_capture: %d\n", la_state.is_continuous_capture);
                        raiseEvent(FLAGS);

                    }
                    break;

                /* ignore any unrecognized bytes. */
                default:
                    log("=>UNHANDLED command: %d", cmd);
                    break;
                
            };
    }
};

} // namespace


