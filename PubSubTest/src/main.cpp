//-----------------------------------------------------------------
/*
      ╔═════════════════════════════════════╗      
      ║            ESP-WROOM-32             ║      
      ║               Devkit                ║      
      ║                                     ║      
      ║EN /                         MOSI/D23║      
      ║VP /A0                        SCL/D22║      
      ║VN /A3                         TX/TX0║      
      ║D34/A6                         RX/RX0║      
      ║D35/A7                        SDA/D21║      
      ║D32/A4,T9                    MISO/D19║      
      ║D33/A5,T8                     SCK/D18║      
      ║D25/A18,DAC1                   SS/ D5║      
      ║D26/A19,DAC2                     /TX2║      
      ║D27/A17,T7                       /RX2║      
      ║D14/A16,T6                 T0,A10/ D4║      
      ║D12/A15,T5     LED_BUILTIN,T2,A12/ D2║
      ║D13/A14,T4                 T3,A13/D15║
      ║GND/                             /GND║
      ║VIN/                             /3V3║
      ║                                     ║      
      ║   EN           μUSB           BOOT  ║      
      ╚═════════════════════════════════════╝      

MQTT/PubSubClient code adapted from:
    https://randomnerdtutorials.com/esp32-mqtt-publish-subscribe-arduino-ide/
*/

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "../../LoopTimer/include/LoopTimer.h"
#include "../../SerialLog/include/SerialLog.h"

LoopTimer loop_timer;

// Bring in WIFI SSID/Password combination, e.g.:
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";
#include "../../wifi_credentials.inc"

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.0.44";

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

#define SUB_TOPIC "esp32/test"

void SetupWifi() 
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
    SerialLog::Log("WiFi connected to IP Address: " + WiFi.localIP().toString());
}

void Callback(char* topic, byte* message, unsigned int length) 
{
    SerialLog::Log("Message arrived on topic: " + String(topic));

    String message_temp;

    for (int i = 0; i < length; i++) 
    {
        message_temp += (char)message[i];
    }
    SerialLog::Log("Message: " + message_temp);

    // Feel free to add more if statements to control more GPIOs with MQTT

    // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
    // Changes the output state according to the message
    if (String(topic) == SUB_TOPIC) 
    {
        if(message_temp == "on")
        {
            SerialLog::Log("<ON>");
        }
        else if(message_temp == "off")
        {
            SerialLog::Log("<OFF>");
        }
    }
}

void Reconnect() 
{
    // Loop until we're reconnected
    while (!mqtt_client.connected()) 
    {
        SerialLog::Log("Attempting MQTT connection...");
        // Attempt to connect
        if (mqtt_client.connect("ESP32Client")) 
        {
            SerialLog::Log("connected");
            // Subscribe
            mqtt_client.subscribe(SUB_TOPIC);
        } 
        else 
        {
            SerialLog::Log("Failed, rc=" + String(mqtt_client.state()));
            SerialLog::Log("Retry in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// This runs on powerup
// -  put your setup code here, to run once:
void setup() 
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
    SetupWifi();
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(Callback);
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() 
{
    loop_timer.Loop();     // typically: 246930 calls/sec or 695400 calls/sec using Ticker

    if (!mqtt_client.connected()) 
    {
        Reconnect();
    }
    mqtt_client.loop();

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
