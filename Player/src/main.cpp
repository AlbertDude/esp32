#include <Arduino.h>
// NOTE: next include needed in this file despite it not being used directly (it is needed by Dac.h)
// - seems that VSCode or platform.io scans only the current folder to determine what libs are needed
// - so if you include a header outside of the current folder, it won't pick up any additional dependencies
// - it should really recurse the headers to see that Dac.h includes this... 
// - or perhaps it has an option to explicitly specify the needed libs?
#include <AudioOutputI2SNoDAC.h>
#include "../../DAC/include/Dac.h"
#include "../../DAC/include/DacVisualizer.h"
#include "../../Switch/include/Switch.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../SerialLog/include/SerialLog.h"

//-----------------------------------------------------------------

//-----------------------------------
// PCM data files
uint8_t buf_meepmeep[] = {
#include "data/meepmeep.dat"
};
uint8_t buf_surely[] = {
#include "data/surely.dat"
};
uint8_t buf_surelySerious[] = {
#include "data/surelySerious.dat"
};
uint8_t buf_surelyShirley[] = {
#include "data/surelyShirley.dat"
};
uint8_t buf_sorryDave[] = {
#include "data/sorryDave.dat"
};
uint8_t buf_pacman[] = {
#include "data/pacman.dat"
};
uint8_t buf_gameOverMan[] = {
#include "data/gameOverMan.dat"
};
const unsigned int kBitDepth = 8;
//-----------------------------------

uint8_t *pcm_bufs[] =
{
    buf_meepmeep,
    buf_surely,
    buf_surelySerious,
    buf_surelyShirley,
    buf_sorryDave,
    buf_pacman,
    buf_gameOverMan,
};

unsigned int pcm_buf_szs[] =
{
    sizeof(buf_meepmeep),
    sizeof(buf_surely),
    sizeof(buf_surelySerious),
    sizeof(buf_surelyShirley),
    sizeof(buf_sorryDave),
    sizeof(buf_pacman),
    sizeof(buf_gameOverMan),
};

const unsigned int kNumBufs = sizeof(pcm_buf_szs)/sizeof(pcm_buf_szs[0]);

//-----------------------------------

// Pick DAC variant to use
// - only define one
// - use of Dac or DacT will default to DAC1 pin output

//#define DAC DacDS
//#define DAC DacT
#define DAC Dac

//-----------------------------------

DAC dac(8000, false /* looped */);
DacVisualizer viz(&dac);
Switch button_switch(T0); // Touch0 = GPIO04
LoopTimer loop_timer;

void setup()
{
    // This runs on powerup
    // put your setup code here, to run once:
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
    pinMode(LED_BUILTIN, OUTPUT); // LED will follow switch state
}

void loop()
{
    // Then this loop runs forever
    // put your main code here, to run repeatedly:
    loop_timer.Loop();     // typically 474986 calls/sec (idling)
                            //          ~340000 calls/sec (cycling thru plays)

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
        static int index = -1;  // increment before use so first usage will be 0

        button_switch.Loop();

        switch(state)
        {
            case kLow:
                if(button_switch.IsHigh())
                {
                    digitalWrite(LED_BUILTIN, HIGH);
                    state = kHigh;
                }
                break;
            case kHigh:
                if(button_switch.IsLow())
                {
                    index++;
                    index = index % kNumBufs;
                    dac.SetBuffer(pcm_bufs[index], pcm_buf_szs[index], kBitDepth);
                    dac.Restart();
                    viz.Reset(&dac);
                    digitalWrite(LED_BUILTIN, LOW);
                    state = kLow;
                }
                break;
            default:
                assert(false);  // Bad State
        };
        dac.Loop();
        viz.Loop();
    }
}

// vim: sw=4:ts=4
