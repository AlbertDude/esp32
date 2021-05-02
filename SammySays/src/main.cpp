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

#include "../../mySAM/include/AudioOutputMonoBuffer.h"
#include "../../SerialLog/include/SerialLog.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../Switch/include/Switch.h"
#include "../../DAC/include/Dac.h"
#include "../../DAC/include/DacVisualizer.h"
#include "../../PubSubTest/include/WifiHelper.h"
#include "../../PubSubTest/include/NtpTime.h"
#include "../../PubSubTest/include/MqttHelper.h"

#define APP_NAME "SammySays"

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
    SerialLog::Log("Sammy says: " + String(phrase));
    out->Reset();
    // This is a blocking call, i.e. buffer is complete upon return
    sam->Say(out, phrase);
//  SerialLog::Log("buf Hz, bsp, #ch: " + String(out->hertz) + ", " + String(out->bps) + ", " + String(out->channels));
//  SerialLog::Log("sample range: " + String(out->min_val_) + " -> " + String(out->max_val_));
//  SerialLog::Log("buf used: " + String(out->GetBufUsed()));
    if(out->GetNumBufOverflows() > 0)
        SerialLog::Log("buf ovrflw: " + String(out->GetNumBufOverflows()));

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

// Wifi Stuff
WiFiClient wifi_client;
#include "../../wifi_credentials.inc"   // defines const char *ssid, *password;

// MQTT Stuff
MqttPubSub<> mqtt_pubsub;
const char* mqtt_server_addr = "192.168.0.44";
MqttLogger mqtt_logger;


//-----------------------------------------------------------------

// This runs on powerup
// -  put your setup code here, to run once:
void setup() {
    // Turn on LED while setting up, connecting to wifi
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
    WifiHelper::Setup(ssid, password);
    NtpTime::Setup();

    mqtt_pubsub.Subscribe(
        APP_NAME"/say", 
        [](String message)
        {
            // message is the phrase to speak
            SayIt(message.c_str());
        }
    );
    mqtt_pubsub.Subscribe(
        APP_NAME"/control", 
        [](String message)
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
                    SetVoice(voice_index);
                }
            }
        }
    );
    mqtt_pubsub.Setup( wifi_client, mqtt_server_addr, APP_NAME );

    mqtt_logger.Setup( &mqtt_pubsub, APP_NAME"/Log" );
    SerialLog::SetSupplementalLogger( &mqtt_logger, "MqttLogger" );

    // SAM generates 22050 Hz, 8 bit, 1 channel

    // In practice, able to dynamically allocate a bigger buffer than can statically allocate
    //  - static data goes into dram0_0_seg which defaults to 124580 bytes
    //      - see ../../output.map
    //  - general usage takes about 40 kBytes, and so we can't match the 110,000
    //  kBytes we've been using via dynamic allocation
    //  - using static buffer might be useful if we're willing to go to the trouble of rejiggin the
    //  default memory layout
#if 1
    out = new AudioOutputMonoBuffer(110000 /* buffSizeSamples */);   // TODO: crappy that we need to set bufSz up front. Better if the TTS lib returned a buffer of appropriate size
#else
    static uint8_t buffer[80000];
    out = new AudioOutputMonoBufferS(80000, buffer);   // TODO: crappy that we need to set bufSz up front. Better if the TTS lib returned a buffer of appropriate size
#endif
    out->begin();
    sam = new ESP8266SAM;

    SayIt("Sammy says, Hello world!");

    // Turn off LED after finished setting up
    digitalWrite(LED_BUILTIN, LOW);
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() {
    loop_timer.Loop();     // by itself, typically 695,400 calls/sec

    // throttle calling rate of Mqtt Loop
    const long kMqttLoopInterval = 5; // Process Mqtt Loop every 5 ms (200 Hz)
    static long prev_attempt = 0;
    long now = millis();
    if (now - prev_attempt > kMqttLoopInterval) 
    {
        mqtt_pubsub.Loop();
        prev_attempt = now;
    }

    dac.Loop();
    viz.Loop();
}

// vim: sw=4:ts=4
