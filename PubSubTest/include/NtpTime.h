//-----------------------------------------------------------------
// NTP Time Helper
//
// - this syncs the Arduino time library with the specified NTP server
//   - configTime() & getLocalTime() are in Arduino.h
// - if running for a long time, might want to do this periodically 
//   to account for any device clock drift
// - subsequent calls to getLocalTime() should, well, get the local time

#include "../../SerialLog/include/SerialLog.h"

namespace NtpTime
{
    // call from setup()
    // - default is for PST (GMT-8)
    void Setup(const char* ntp_server="pool.ntp.org", long gmt_offset_sec=-8 * 3600, int dst_offset_sec=3600) 
    {
        // Init and get the time
        configTime(gmt_offset_sec, dst_offset_sec, ntp_server);

        tm timeinfo;
        if(!getLocalTime(&timeinfo))
        {
            SerialLog::Log("Failed to get time");
            return;
        }
        SerialLog::Log(timeinfo, "%A, %B %d %Y %H:%M:%S");  // strftime format specification
        SerialLog::UseLocalTime();  // Switch SerialLog to log the actual date-time
        SerialLog::Log("Switched SerialLog to report local time");
    }

}

