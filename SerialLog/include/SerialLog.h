#pragma once

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

// Modified from getLocalTime() in esp32-hal-time.c
bool GetLocalTimeWithMs(struct tm * info, int &ms, uint32_t timeout_ms=5000);
bool GetLocalTimeWithMs(struct tm * info, int &ms, uint32_t timeout_ms)
{
    uint32_t start = millis();
    time_t now;
    timeval tval;
    while((millis()-start) <= timeout_ms) {
        gettimeofday(&tval, NULL);
        now = tval.tv_sec;
        localtime_r(&now, info);
        if(info->tm_year > (2016 - 1900)){
            ms = tval.tv_usec / 1000;
            return true;
        }
        delay(10);
    }
    return false;
}

// Interface for supplemental logger
class ILogger
{
public:
    virtual void DoLog(String msg) = 0;
};

// Logging helper
// - wrapper around Serial.prints with timestamps
//  - note that timestamps are referenced vs. the first call to SerialLog::log()
//  - so recommend to do a SerialLog::log("Hello World") at startup
// - singleton with lazy initialization following singleton implementation notes from:
// https://stackoverflow.com/questions/1008019/c-singleton-design-pattern
// https://stackoverflow.com/questions/86582/singleton-how-should-it-be-used
// - note that as mentioned in the refs, this is not thread-safe
class SerialLog
{
public:
    static void Log(String msg)
    {
        SerialLog::GetInstance().DoLog(msg);
    }

    static void Log(tm & timeinfo, const char * format)
    {
        SerialLog::GetInstance().DoLog(timeinfo, format);
    }

    static void UseLocalTime()
    {
        SerialLog::GetInstance().use_local_time_ = true;
    }

    static void SetSupplementalLogger(ILogger * logger, const char * name = "")
    {
        SerialLog::GetInstance().supplemental_logger_ = logger;
        Log(String("Added supplemental Logger: ") + name);
    }

public:
    // Prevent copies of the singleton by preventing use of copy constructor and assignment operator
    // - public access for supposedly better error messaging
    SerialLog(SerialLog const&)       = delete;
    void operator=(SerialLog const&)  = delete;

private:
    unsigned long start_millis_;
    bool use_local_time_ = false;
    ILogger * supplemental_logger_ = nullptr;

    SerialLog()
    {
        start_millis_ = millis();
    }

    static SerialLog& GetInstance()
    {
        static SerialLog    instance;   // Guaranteed to be destroyed.
                                        // Instantiated on first use.
        return instance;
    }

    String GetElapsedTime()
    {
        // TODO: do wraparound check, though that takes ~ 50 days to happen
        // - uint32_t => 49.7 days
        // https://www.arduino.cc/reference/en/language/functions/time/millis/
        unsigned long elapsed = millis() - start_millis_;
        float elapsed_secs = (float)(elapsed)/1000.f;
        String timestamp = String(elapsed_secs, 3) + "> ";
        String pad_str = "";
        const char *pads[] = {"", "0", "00", "000"};
        int pad_len = 10 - timestamp.length();
        assert (pad_len < (sizeof(pads)/sizeof(pads[0])));
        return pads[pad_len] + timestamp;
    }

    String GetLogTime()
    {
        if (use_local_time_)
        {
            // Some hoops to go thru to display localtime with ms...
            tm timeinfo;
            int ms;
            if (GetLocalTimeWithMs(&timeinfo, ms))
            {
                char time_str[30];
                strftime(time_str, 30, "%y-%m-%d %H:%M:%S.abc", &timeinfo);
                // Oh, the horror! -- of the next 2 lines of code
                const int index = 17;
                snprintf(time_str+index, sizeof(time_str)-index-1, ".%03d> ", ms);
                return String(time_str);
            }
        }
        return GetElapsedTime();
    }

    void DoLog(String msg)
    {
        String log_time = GetLogTime();
        Serial.println( log_time + msg );
        if( supplemental_logger_ )
        {
            supplemental_logger_->DoLog( log_time + msg );
        }
    }

    void DoLog(tm & timeinfo, const char * format)
    {
        String log_time = GetLogTime();
        char time_str[128];
        strftime(time_str, 128, format, &timeinfo);
        Serial.println( log_time + String(time_str) );
        if( supplemental_logger_ )
        {
            supplemental_logger_->DoLog( log_time + time_str );
        }
    }
};

