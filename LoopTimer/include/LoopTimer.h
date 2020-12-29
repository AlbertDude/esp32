#pragma once

#include "../../SerialLog/include/SerialLog.h"

// Performance profiling for loop()
// - Reports on number of calls/sec over the specified reporting interval
class LoopTimer
{
public:
    LoopTimer(unsigned reportingInterval_ms = 5000)
    : m_reportingInterval_ms(reportingInterval_ms)
    {
    }

    void loop()
    {
        unsigned long now = millis();
        if(m_prevReporting_ms == 0)
            m_prevReporting_ms = now;

        m_callCount++;
        if(now >= m_prevReporting_ms + m_reportingInterval_ms)
        {
            SerialLog::log("Over past period (" + String(m_reportingInterval_ms) +
                    " ms), loop() rate (call/s) = " + String((float)m_callCount / m_reportingInterval_ms * 1000.f) );
            m_prevReporting_ms = now;
            m_callCount = 0;
        }
    }
private:
    unsigned m_reportingInterval_ms;
    unsigned long m_prevReporting_ms = 0;
    unsigned long m_callCount = 0;
};
// vim: sw=4:ts=4
