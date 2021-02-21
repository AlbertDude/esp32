#include <Arduino.h>
// NOTE: next include needed in this file despite it not being used directly (it is needed by Dac.h)
// - seems that VSCode or platform.io or Arduino ?scans this file? to determine what libs are needed
// - it should really recurse the headers to see that Dac.h includes this... or the needed libs
// should be explicitly listed
#include <AudioOutputI2SNoDAC.h>
#include "../../DAC/include/Dac.h"
#include "../../Switch/include/Switch.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../SerialLog/include/SerialLog.h"

//-----------------------------------------------------------------

//-----------------------------------
// PCM data files
uint8_t meepMeepBuf[] = {
#include "data/meepmeep.dat"
};
uint8_t surelyBuf[] = {
#include "data/surely.dat"
};
uint8_t surelySeriousBuf[] = {
#include "data/surelySerious.dat"
};
uint8_t surelyShirleyBuf[] = {
#include "data/surelyShirley.dat"
};
uint8_t sorryDaveBuf[] = {
#include "data/sorryDave.dat"
};
uint8_t pacmanBuf[] = {
#include "data/pacman.dat"
};
uint8_t gameOverManBuf[] = {
#include "data/gameOverMan.dat"
};
//-----------------------------------

uint8_t *pcmBufs[] =
{
    meepMeepBuf,
    surelyBuf,
    surelySeriousBuf,
    surelyShirleyBuf,
    sorryDaveBuf,
    pacmanBuf,
    gameOverManBuf,
};

unsigned int pcmBufSzs[] =
{
    sizeof(meepMeepBuf),
    sizeof(surelyBuf),
    sizeof(surelySeriousBuf),
    sizeof(surelyShirleyBuf),
    sizeof(sorryDaveBuf),
    sizeof(pacmanBuf),
    sizeof(gameOverManBuf),
};

unsigned int numBufs = sizeof(pcmBufSzs)/sizeof(pcmBufSzs[0]);

DacT dac(DAC1, 8000, false);    // one-shot
Switch buttonSwitch(T0); // Touch0 = GPIO04
LoopTimer loopTimer;

void setup()
{
    // This runs on powerup
    // put your setup code here, to run once:
    Serial.begin(115200); // for serial link back to computer
    SerialLog::log(__FILE__);
    pinMode(LED_BUILTIN, OUTPUT); // LED will follow switch state
}

void loop()
{
    // Then this loop runs forever
    // put your main code here, to run repeatedly:
    loopTimer.loop();     // typically 474986 calls/sec (idling)
                            //          ~340000 calls/sec (cycling thru plays)

    //dac.loop(); // not needed for DacT

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
                    index++;
                    index = index % numBufs;
                    dac.setBuffer(pcmBufs[index], pcmBufSzs[index]);
                    dac.restart();
                    digitalWrite(LED_BUILTIN, LOW);
                    state = State_Low;
                }
                break;
            default:
                assert(false);  // Bad State
        };
    }
}

// vim: sw=4:ts=4
