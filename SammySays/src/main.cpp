// TODO:
// - create phrase queue
// - playback items in queue with <GAP> between each entry

/* SammySays
 *
 * TTS (based on SAM) with text and control input via MQTT
 *
 * H/W setup as per DAC project

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
        DAC1──║D25/A18,DAC1                   SS/ D5║      
              ║D26/A19,DAC2                     /TX2║      
              ║D27/A17,T7                       /RX2║      
              ║D14/A16,T6                 T0,A10/ D4║      
              ║D12/A15,T5     LED_BUILTIN,T2,A12/ D2║
              ║D13/A14,T4                 T3,A13/D15║
              ║GND/                             /GND║
         VIN──║VIN/                             /3V3║
              ║                                     ║      
              ║   EN           μUSB           BOOT  ║      
              ╚═════════════════════════════════════╝      

Driving speaker from 8-bit DAC output + LM386 amplifier:
See: https://hackaday.com/2016/12/07/you-can-have-my-lm386s-when-you-pry-them-from-my-cold-dead-hands/

                        ╔═════════════════╗        
       220u +           ║      LM386      ║        
    ╲║────C──┬──────────║Vout          GND║─┬─┐       DAC1 (from ESP DevKit)
    ╱║─┐   C,47n 5V,VIN─║Vs            IN+║─┘ ▽        │
SPKR   ▽     │      ┌───║BYPASS        IN-║───────────▶R,10k
           R,10  C,100n ║GAIN,8     1,GAIN║            │
             │      │   ║        ◯        ║            ▽
             ▽      ▽   ╚═════════════════╝        
 *
 */

#include <Arduino.h>
#include <ESP8266SAM.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "../../mySAM/include/AudioOutputMonoBuffer.h"
#include "../../SerialLog/include/SerialLog.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../Switch/include/Switch.h"
#include "../../DAC/include/Dac.h"
#include "../../DAC/include/DacVisualizer.h"

//-----------------------------------------------------------------
// Dac and SAM helpers

// Pick DAC variant to use
// - only define one
// - use of Dac or DacT will default to DAC1 pin output

//#define DAC DacDS
//#define DAC DacT
#define DAC Dac

DAC dac(22050, false /* looped */);
DacVisualizer viz;
LoopTimer loop_timer;
Switch button_switch(T0); // Touch0 = GPIO04

AudioOutputMonoBuffer *out = nullptr;
ESP8266SAM *sam = nullptr;

const ESP8266SAM::SAMVoice voices[] = {
  ESP8266SAM::VOICE_SAM, 
  ESP8266SAM::VOICE_ELF, 
  ESP8266SAM::VOICE_ROBOT,          // kinda like this one
  ESP8266SAM::VOICE_STUFFY, 
  ESP8266SAM::VOICE_OLDLADY,        // and this one
  ESP8266SAM::VOICE_ET
};
const unsigned int kNumVoices = sizeof(voices)/sizeof(voices[0]);
int voice_index = -1;
const char* kVoiceNames[] = {
  "SAM", 
  "ELF", 
  "ROBOT", 
  "STUFFY", 
  "OLDLADY", 
  "ET"
};

void SayIt(const char* phrase)
{
    out->Reset();
    // This is a blocking call, i.e. buffer is complete upon return
    sam->Say(out, phrase);
    SerialLog::Log("buf Hz, bsp, #ch: " + String(out->hertz) + ", " + String(out->bps) + ", " + String(out->channels));
    SerialLog::Log("buf used: " + String(out->GetBufUsed()));
    SerialLog::Log("buf ovrflw: " + String(out->GetNumBufOverflows()));
    SerialLog::Log("sample range: " + String(out->min_val_) + " -> " + String(out->max_val_));

    dac.SetBuffer(out->GetBuf(), out->GetBufUsed(), out->bps);
    dac.Restart();
    viz.Reset(&dac);
}

void SetVoice(int voice_index)
{
    voice_index %= kNumVoices;
    sam->SetVoice(voices[voice_index]);
    SerialLog::Log("Setting Voice: " + String(kVoiceNames[voice_index]));
}

void HelpVoices()
{
    for(int i=0; i<kNumVoices; i++)
    {
        SerialLog::Log("voice: " + String(i) + " = " + String(kVoiceNames[i]));
    }
}

//-----------------------------------------------------------------
// Wifi and MQTT helpers

// Bring in WIFI SSID/Password combination, e.g.:
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";
#include "../../wifi_credentials.inc"

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.0.44";

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
const long kReconnectAttemptInterval = 5000; // Try reconnections every 5 s

// Subscribe topics
#define TOPIC_SAY "SammySays/say"
#define TOPIC_CONTROL "SammySays/control"

// Publish topics
// NONE

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

void Callback(char* topic, byte* message_bytes, unsigned int length) 
{
    SerialLog::Log("Message arrived on topic: " + String(topic));

    String message;

    for (int i = 0; i < length; i++) 
    {
        message += (char)message_bytes[i];
    }
    message.trim(); // remove leading/trailing whitespace
    SerialLog::Log("Message: " + message);

    if (String(topic) == TOPIC_SAY) 
    {
        // message is the phrase to speak
        SayIt(message.c_str());
    }
    else if (String(topic) == TOPIC_CONTROL) 
    {
        // "voice N"
        // "voice ?"
        if (message.startsWith("voice"))
        {
            message.remove(0, message.indexOf(' ')+1);

            if (message.startsWith("?"))
                HelpVoices();
            else
            {
                int voice_index = message.toInt();
                SerialLog::Log("Voice index: " + String(voice_index));
                SetVoice(voice_index);
            }
        }
    }
}

boolean ReconnectNonBlocking() 
{
    bool connected = mqtt_client.connect("ESP32Client");
    if (connected)
    {
        SerialLog::Log("connected");
        // subscriptions
        mqtt_client.subscribe(TOPIC_SAY);
        mqtt_client.subscribe(TOPIC_CONTROL);
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

// This runs on powerup
// -  put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);

    SetupWifi();
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(Callback);

    pinMode(LED_BUILTIN, OUTPUT);

    // SAM generates 22050 Hz, 8 bit, 1 channel
    out = new AudioOutputMonoBuffer(110000 /* buffSizeSamples */);   // TODO: crappy that we need to set bufSz up front. Better if the TTS lib returned a buffer of appropriate size
    out->begin();
    sam = new ESP8266SAM;

    // Initial mqtt connection
    ReconnectBlocking();
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() {
    loop_timer.Loop();     // by itself, typically 695,400 calls/sec

    // throttle calling rate of MqttLoop
    // Call rates for different throttling rates
    //  339,000 ( 5ms, 200Hz)
    //  341,000 (10ms, 100Hz)
    //  342,000 (17ms,  60Hz)
    const long kMqttLoopInterval = 5; // Process MqttLoop every 5 ms (200 Hz)
    static long prev_attempt = 0;
    long now = millis();
    if (now - prev_attempt > kMqttLoopInterval) 
    {
        MqttLoop();
        prev_attempt = now;
    }

    dac.Loop();
    viz.Loop();
}

// vim: sw=4:ts=4
