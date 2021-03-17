#include <Arduino.h>
#include <ESP8266SAM.h>

#include "AudioOutputMonoBuffer.h"
#include "../../SerialLog/include/SerialLog.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../Switch/include/Switch.h"
#include "../../DAC/include/Dac.h"
#include "../../DAC/include/DacVisualizer.h"

//-----------------------------------------------------------------

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

// This runs on powerup
// -  put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
    SerialLog::Log("in setup(), Voice Index: " + String(voice_index));

    pinMode(LED_BUILTIN, OUTPUT); // LED will follow switch state

    // SAM generates 22050 Hz, 8 bit, 1 channel
    out = new AudioOutputMonoBuffer(90000 /* buffSizeSamples */);   // TODO: crappy that we need to set bufSz up front. Better if the TTS lib returned a buffer of appropriate size
    out->begin();
    sam = new ESP8266SAM;
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() {

    loop_timer.Loop();      // base-rate: 695,482 calls/sec
                            // - drops to around 260,000 when hammering the switch to get SAM
                            // running

    // for these phrases, uses up to ~46000 samples (for standard voice)
    // - buffer size is voice-dependent
    const char* phrases[] = {
        "Can you hear me now?",
        "I cannot hear you!",
        "what, is your name?",
        "hello bethany",
        "hello emerson",
        "how old are you",
        "don't call me, i'll call you",
    };
    const unsigned int kNumPhrases = sizeof(phrases)/sizeof(phrases[0]);

    // button-handling
    // Assumes: normally-LOW
    // - ->HIGH->LOW triggers advancing to next clip
    {
        enum State
        {
            kLow,
            kHigh
        };
        static State state = kLow;
        static int phrase_index = -1;  // increment before use so first usage will be 0

        button_switch.Loop();

        switch(state)
        {
            case kLow:
                if(button_switch.IsHigh())
                {
                    //digitalWrite(LED_BUILTIN, HIGH);
                    state = kHigh;
                }
                break;
            case kHigh:
                if(button_switch.IsLow())
                {
                    //digitalWrite(LED_BUILTIN, LOW);
                    out->Reset();
                    phrase_index++;
                    phrase_index = phrase_index % kNumPhrases;
                    if(phrase_index == 0)
                    {
                        voice_index++;
                        voice_index = voice_index % kNumVoices;
                        sam->SetVoice(voices[voice_index]);
                        SerialLog::Log("====================");
                        SerialLog::Log("Setting Voice: " + String(kVoiceNames[voice_index]));
                    }

                    SerialLog::Log("--------------------");
                    SerialLog::Log("Phrase: " + String(phrases[phrase_index]));

                    // This is a blocking call, i.e. buffer is complete upon return
                    sam->Say(out, phrases[phrase_index]);
                    SerialLog::Log("buf Hz, bsp, #ch: " + String(out->hertz) + ", " + String(out->bps) + ", " + String(out->channels));
                    SerialLog::Log("buf used: " + String(out->GetBufUsed()));
                    SerialLog::Log("buf ovrflw: " + String(out->GetNumBufOverflows()));
                    SerialLog::Log("sample range: " + String(out->min_val_) + " -> " + String(out->max_val_));

                    dac.SetBuffer(out->GetBuf(), out->GetBufUsed(), out->bps);
                    dac.Restart();
                    viz.Reset(&dac);
                    state = kLow;
                }
                break;
            default:
                assert(false);  // Bad State
        };
    }
    dac.Loop();
    viz.Loop();
}

// vim: sw=4:ts=4
