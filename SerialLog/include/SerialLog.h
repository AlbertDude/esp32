#pragma once

#include <Arduino.h>

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
    static void log(String msg)
    {
        SerialLog::getInstance().doLog(msg);
    }

public:
    // Prevent copies of the singleton by preventing use of copy constructor and assignment operator
    // - public access for supposedly better error messaging
    SerialLog(SerialLog const&)       = delete;
    void operator=(SerialLog const&)  = delete;

private:
    unsigned long m_startMs;
    SerialLog()
    {
        m_startMs = millis();
    }

    static SerialLog& getInstance()
    {
        static SerialLog    instance;   // Guaranteed to be destroyed.
                                        // Instantiated on first use.
        return instance;
    }

    void doLog(String msg)
    {
        // TODO: do wraparound check, though that takes ~ 50 days to happen
        // - uint32_t => 49.7 days
        // https://www.arduino.cc/reference/en/language/functions/time/millis/
        unsigned long elapsed = millis() - m_startMs;
        float elapsed_secs = (float)(elapsed)/1000.f;
        Serial.println( String(elapsed_secs, 3) + ": " + msg );
    }

};

