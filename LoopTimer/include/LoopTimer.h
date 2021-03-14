#pragma once

#include "../../SerialLog/include/SerialLog.h"

// Performance profiling for loop()
// - Reports on number of calls/sec over the specified reporting interval
class LoopTimer
{
public:
    LoopTimer(unsigned reportingInterval_ms = 5000)
        : reporting_interval_millis_(reportingInterval_ms)
    {
    }

    void Loop()
    {
        unsigned long now = millis();
        if(prev_reporting_millis_ == 0)
            prev_reporting_millis_ = now;

        call_count_++;
        if(now >= prev_reporting_millis_ + reporting_interval_millis_)
        {
            SerialLog::Log("Over past period (" + String(reporting_interval_millis_) +
                    " ms), loop() rate (call/s) = " + String((float)call_count_ / reporting_interval_millis_ * 1000.f) );
            prev_reporting_millis_ = now;
            call_count_ = 0;
        }
    }
private:
    unsigned reporting_interval_millis_;
    unsigned long prev_reporting_millis_ = 0;
    unsigned long call_count_ = 0;
};
// vim: sw=4:ts=4
