//-----------------------------------------------------------------
// WIFI setup helper
//
// - note if wifi not connecting, try pressing 'Reset' button

#include <WiFi.h>
#include "../../SerialLog/include/SerialLog.h"

namespace WifiHelper
{
    // call from setup()
    void Setup(const char* ssid, const char* password) 
    {
        delay(10);
        // We start by connecting to a WiFi network
        SerialLog::Log("Connecting to " + String(ssid));

        WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED) 
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        SerialLog::Log("WiFi connected to IP Address: " + WiFi.localIP().toString());
    }
}
