// TODO:
// - create MqttHelper.h
// - publish log to mqtt topic: PubSubTest/log

/* PubSubTest
 *
 * MQTT sample 
 * - subscribes to `PubSubTest/test` for "on" and "off" messages
 *
 * Also serves as samples for these helpers:
 * NtpTime
 * - syncs device local time with NTP server
 * WifiHelper
 * - helper to setup Wifi
 * - note if wifi not connecting, try pressing 'Reset' button
 *
 * No specific H/W setup needed
 *
 * - MQTT/PubSubClient code adapted from:
 *      https://randomnerdtutorials.com/esp32-mqtt-publish-subscribe-arduino-ide/
 */

#include <Arduino.h>
#include <PubSubClient.h>   // For MQTT support

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
MqttHelper<> mqtt_helper;
const char* mqtt_server_addr = "192.168.0.44";


// This runs on powerup
// -  put your setup code here, to run once:
void setup() 
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(APP_NAME" says Hello");
    WifiHelper::Setup(ssid, password);
    NtpTime::Setup();
    mqtt_helper.Subscribe(
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
    mqtt_helper.Subscribe(
        APP_NAME"/echo", 
        [](String message)
        {
            SerialLog::Log("ECHO: " + message);
        }
    );
    mqtt_helper.Setup(wifi_client, mqtt_server_addr, APP_NAME);
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
        mqtt_helper.Loop();
        prev_attempt = now;
    }

    // Publish every 5s
//  static long lastMsg = 0;
//  long now = millis();
//  if (now - lastMsg > 5000) 
//  {
//      lastMsg = now;

//      // Convert the value to a char array
//      char tempString[8];
//      dtostrf(temperature, 1, 2, tempString);
//      Serial.print("Temperature: ");
//      Serial.println(tempString);
//      mqtt_helper.Publish("esp32/temperature", tempString);
//  }
}

// vim: sw=4:ts=4


