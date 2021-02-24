#include <Arduino.h>
#include <ESP8266SAM.h>

#include <AudioOutputMonoBuffer.h>

#include "../../SerialLog/include/SerialLog.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../Switch/include/Switch.h"
#include "../../DAC/include/Dac.h"

//-----------------------------------------------------------------

DacT dac(DAC1, 22050, false);    // one-shot
LoopTimer loopTimer;
Switch buttonSwitch(T0); // Touch0 = GPIO04

AudioOutputMonoBuffer *out = NULL;
ESP8266SAM *sam = nullptr;

/*
  enum SAMVoice { VOICE_SAM, VOICE_ELF, VOICE_ROBOT, VOICE_STUFFY, VOICE_OLDLADY, VOICE_ET };
  void SetVoice(enum SAMVoice voice);
*/
const ESP8266SAM::SAMVoice voices[] = {
  ESP8266SAM::VOICE_SAM, 
  ESP8266SAM::VOICE_ELF, 
  ESP8266SAM::VOICE_ROBOT, 
  ESP8266SAM::VOICE_STUFFY, 
  ESP8266SAM::VOICE_OLDLADY, 
  ESP8266SAM::VOICE_ET
};
const unsigned int numVoices = sizeof(voices)/sizeof(voices[0]);
int voiceIndex = -1;
const char* voiceNames[] = {
  "SAM", 
  "ELF", 
  "ROBOT", 
  "STUFFY", 
  "OLDLADY", 
  "ET"
};


void setup() {
    // This runs on powerup
    // put your setup code here, to run once:
    Serial.begin(115200); // for serial link back to computer
    SerialLog::log(__FILE__);

    pinMode(LED_BUILTIN, OUTPUT); // LED will follow switch state

    // SAM generates 22050 Hz, 8 bit, 1 channel
    out = new AudioOutputMonoBuffer(90000 /* buffSizeSamples */);   // TODO: crappy that we need to set bufSz up front. Better if the TTS lib returned a buffer of appropriate size
    out->begin();
    sam = new ESP8266SAM;
}

void loop() {
    // Then this loop runs forever
    // put your main code here, to run repeatedly:

    loopTimer.loop();     // typically 695482 calls/sec

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
        // TODO: there's a bug in the libs that causes this to repeat at the end...
        // - suspect it's in the I2SNoDac implementation as it sounds like a partial buffer gets
        // looped indefinitely
    };
    const unsigned int numPhrases = sizeof(phrases)/sizeof(phrases[0]);

    // button-handling
    // Assumes: normally-LOW
    // - ->HIGH->LOW triggers advancing to next clip
    {
        enum State
        {
            State_Low,
            State_High
        };
        static State state = State_Low;
        static int index = -1;  // increment before use so first usage will be 0

        buttonSwitch.loop();

        switch(state)
        {
            case State_Low:
                if(buttonSwitch.isHigh())
                {
                    digitalWrite(LED_BUILTIN, HIGH);
                    state = State_High;
                }
                break;
            case State_High:
                if(buttonSwitch.isLow())
                {
                    digitalWrite(LED_BUILTIN, LOW);
                    index++;

                    out->reset();
                    index = index % numPhrases;

                    if(index == 0)
                    {
                        voiceIndex++;
                        voiceIndex = voiceIndex % numVoices;
                        sam->SetVoice(voices[voiceIndex]);
                        SerialLog::log("====================");
                        SerialLog::log("Setting Voice: " + String(voiceNames[voiceIndex]));
                    }

                    SerialLog::log("--------------------");
                    SerialLog::log("Phrase: " + String(phrases[index]));

                    // This seems to be a blocking call
                    // i.e. buffer is complete upon return
                    sam->Say(out, phrases[index]);
                    SerialLog::log("buf Hz, bsp, #ch: " + String(out->hertz) + ", " + String(out->bps) + ", " + String(out->channels));
                    SerialLog::log("buf used: " + String(out->GetBufUsed()));
                    SerialLog::log("buf ovrflw: " + String(out->GetNumBufOverflows()));
                    SerialLog::log("sample range: " + String(out->minVal) + " -> " + String(out->maxVal));

                    dac.setBuffer(out->GetBuf(), out->GetBufUsed());
                    dac.restart();
                    state = State_Low;
                }
                break;
            default:
                assert(false);  // Bad State
        };
    }
}

// vim: sw=4:ts=4
