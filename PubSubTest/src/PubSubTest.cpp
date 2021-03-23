/* PubSubTest
 *
 * MQTT sample 
 * - subscribes to `PubSubTest/test` for "on" and "off" messages
 * - subscribes to `PubSubTest/echo` echoing any published message
 * - publishes to `PubSubTest/Log` duplicating all content logged to SerialLog
 *
 * Serves as samples for these helpers:
 * NtpTime
 * - syncs device local time with NTP server
 * WifiHelper
 * - helper to setup Wifi
 * - note if wifi not connecting, try pressing 'Reset' button
 * MqttHelper
 * - MqttPubSub - MQTT subscribing and publishing
 * - MqttLogger - acts as supplemental logger for SerialLog
 *
 * No specific H/W setup needed
 */

#include <Arduino.h>

#include "../../LoopTimer/include/LoopTimer.h"
#include "../../SerialLog/include/SerialLog.h"
#include "../../PubSubTest/include/WifiHelper.h"
#include "../../PubSubTest/include/NtpTime.h"
#include "../../PubSubTest/include/MqttHelper.h"


#define APP_NAME "PubSubTest"

//-----------------------------------------------------------------

LoopTimer loop_timer;

// Wifi Stuff
WiFiClient wifi_client;
#include "../../wifi_credentials.inc"   // defines const char *ssid, *password;

// MQTT Stuff
MqttPubSub<> mqtt_pubsub;
const char* mqtt_server_addr = "192.168.0.44";
MqttLogger mqtt_logger;


// This runs on powerup
// -  put your setup code here, to run once:
void setup() 
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(APP_NAME" says Hello");
    WifiHelper::Setup(ssid, password);
    NtpTime::Setup();
    mqtt_pubsub.Subscribe(
        APP_NAME"/onoff", 
        [](String message)
        {
            if(message == "on")
            {
                SerialLog::Log("<ON>");
            }
            else if(message == "off")
            {
                SerialLog::Log("<OFF>");
            }
        }
    );
    mqtt_pubsub.Subscribe(
        APP_NAME"/echo", 
        [](String message)
        {
            SerialLog::Log("ECHO: " + message);
        }
    );
    mqtt_pubsub.Setup( wifi_client, mqtt_server_addr, APP_NAME );

    mqtt_logger.Setup( &mqtt_pubsub, APP_NAME"/Log" );
    SerialLog::SetSupplementalLogger( &mqtt_logger, "MqttLogger" );
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() 
{
    loop_timer.Loop();     // by itself, typically 695,400 calls/sec

    // throttle calling rate of MqttLoop
    const long kMqttLoopInterval = 5; // Process MqttLoop every 5 ms (200 Hz)
    static long prev_attempt = 0;
    long now = millis();
    if (now - prev_attempt > kMqttLoopInterval) 
    {
        mqtt_pubsub.Loop();
        prev_attempt = now;
    }
}

// vim: sw=4:ts=4


