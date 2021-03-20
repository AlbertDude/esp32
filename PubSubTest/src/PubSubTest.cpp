// TODO:
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
// - note if wifi not connecting, try pressing 'Reset' button
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


#define APP_NAME "PubSubTest"

//-----------------------------------------------------------------
// MQTT stuff

// MQTT Broker IP address
const char* mqtt_server = "192.168.0.44";

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
const long kReconnectAttemptInterval = 5000; // Try reconnections every 5 s

#define TOPIC_SUB_TEST APP_NAME"/test"

void Callback(char* topic, byte* message_bytes, unsigned int length) 
{
    SerialLog::Log("Message arrived on topic: " + String(topic));

    String message;

    for (int i = 0; i < length; i++) 
    {
        message += (char)message_bytes[i];
    }
    SerialLog::Log("Message: " + message);

    if (String(topic) == TOPIC_SUB_TEST) 
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
}

boolean ReconnectNonBlocking() 
{
    bool connected = mqtt_client.connect(APP_NAME);
    if (connected)
    {
        SerialLog::Log("MQTT connected");
        // subscriptions
        SerialLog::Log("MQTT subscribed topics: " + String(TOPIC_SUB_TEST));
        mqtt_client.subscribe(TOPIC_SUB_TEST);
    }
    return connected;
}

void ReconnectBlocking() 
{
    // Loop until we're reconnected
    while (!mqtt_client.connected()) 
    {
        SerialLog::Log("Attempting MQTT connection...");
        // Attempt to connect
        if (!ReconnectNonBlocking())
        {
            SerialLog::Log("Failed, rc=" + String(mqtt_client.state()));
            SerialLog::Log("Retry in 5 seconds");
            delay(kReconnectAttemptInterval); // Wait before retrying
        }
    }
}

// call from setup()
void MqttSetup()
{
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(Callback);

    // Initial mqtt connection
    ReconnectBlocking();
}

// call from loop()
void MqttLoop()
{
    if (!mqtt_client.connected()) 
    {
        static long prev_attempt = 0;
        long now = millis();
        if (now - prev_attempt > kReconnectAttemptInterval) 
        {
            // Attempt to reconnect
            ReconnectNonBlocking();
            prev_attempt = now;
        }
    } 
    else 
    {
        mqtt_client.loop();
    }
}

//-----------------------------------------------------------------

LoopTimer loop_timer;

// Bring in WIFI SSID/Password combination, e.g.:
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";
#include "../../wifi_credentials.inc"

// This runs on powerup
// -  put your setup code here, to run once:
void setup() 
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(APP_NAME" says Hello");
    WifiHelper::Setup(ssid, password);
    NtpTime::Setup();
    MqttSetup();
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
        MqttLoop();
        prev_attempt = now;
    }

    // Publish every 5s
//  static long lastMsg = 0;
//  long now = millis();
//  if (now - lastMsg > 5000) {
//      lastMsg = now;

//      // Convert the value to a char array
//      char tempString[8];
//      dtostrf(temperature, 1, 2, tempString);
//      Serial.print("Temperature: ");
//      Serial.println(tempString);
//      mqtt_client.publish("esp32/temperature", tempString);
//  }
}

// vim: sw=4:ts=4
