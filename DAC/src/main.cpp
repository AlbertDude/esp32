
/*
              ╔═════════════════════════════════════╗      
              ║            ESP-WROOM-32             ║      
              ║               Devkit                ║      
              ║                                     ║      
              ║EN /                         MOSI/D23║      
              ║VP /A0                        SCL/D22║──I2SOut
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

- Two options:
    DacT - timer-based driver
        - good up to about 16 kHz (w/ either 8/16 bit data buffer)
            - beyond that, playback seems slowed down, guessing it's a limitation of the timer rate
    Dac  - polling-based driver (need to regularly call ::loop())
        - good up to about 22 kHz (w/ 8 bit data buffer) before hear slow down
            - hear pops at all rates though -- seems to be caused by serial prints
            - realistically, this would make this implementation not-usable in any real apps

- TODO: characterize sound-quality vs. DacDS implementation (8-bit data only)
    - incomplete evaluation
    - prelim thoughts:
        - 8 bit DAC sounds better
            - smoother, less harsh
        - i.e. the DacDS seems to have a harsh sound to it


Driving speaker from delta-sigma I2S output + output transistor and resistor:
See: https://github.com/earlephilhower/ESP8266Audio/#software-i2s-delta-sigma-dac-ie-playing-music-with-a-single-transistor-and-speaker

                            ╔═════════╗        
                           ╔╝  2N3904 ╚╗     
                           ║   (NPN)   ║        
                           ║           ║        
                           ║  E  B  C  ║         
                           ╚═══════════╝           
                              │  │  └──────║╱
                              ▽  │     ┌───║╲
        I2SOut ────────R─────────┘     │      SPKR 
  (from ESP DevKit)    1k            5V,VIN            

- One option:
    DacDS  - polling-based driver (need to regularly call ::loop())
        - good all the way up to 44.1 kHz (w/ 8 bit data buffer)
        - good all the way up to 44.1 kHz (w/ 16 bit data buffer)
            - lower samplerates sound both noisy and distorted in the high freqs
            - starts to sound decent @ 24 kHz, but even so the steps to 32 kHz and then 44.1 kHz both easy to discern improved sound quality

- TODO: characterize sound-quality 8-bit vs 16-bit
- TODO: implement ticker-based implementation
    - based on experience with Dac & DacT, seems like ticker won't be able to support higher samplerates

*/

#include <Arduino.h>
#include "Dac.h"
#include "DacVisualizer.h"
#include "../../SerialLog/include/SerialLog.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../Switch/include/Switch.h"


// Test function for:
// - initial demonstration of outputting to DAC
// - generating (slow) output ramp to help characterize DAC output levels & linearity
//
// DAC output measurements
// - board powered from 5V source, no blinking LEDs
// - 3v3 pin measured 3.27V
//
// value  DAC1  DAC2 (mV)
//   0      83	 112
//  64	   858	 877
// 128	  1610	1645
// 192	  2370	2400
// 255    3130	3160
void dac_ramp()
{
    const static uint8_t OUTPUT_MIN = 56;  // corresponds to output voltage ~0.75V
    static uint8_t outputValue = OUTPUT_MIN;
    static unsigned long m_prevToggle = 0;
    static unsigned long m_interval = 20; // increment/decrement interval
    //static unsigned long m_interval = 7000; // for characterization
    static bool incrementing = true;

    unsigned long now = millis();
    if((m_prevToggle == 0) || (now >= m_prevToggle + m_interval))
    {
        dacWrite(DAC1, outputValue);
        //SerialLog::log("DAC value: " + String(outputValue));
        if(incrementing)
        {
            outputValue++;
            if(outputValue==255){
                incrementing = false;
            }
        }
        else
        {
            outputValue--;
            if(outputValue==OUTPUT_MIN)
            {
                incrementing = true;
            }
        }
        m_prevToggle = now;
    }
}


// Pick test mode
// - only define one
#define TEST_MODE_CYCLE_8B_SAMPRATES
//#define TEST_MODE_CYCLE_16B_SAMPRATES
//#define TEST_MODE_CYCLE_BITDEPTHS

// Pick DAC variant to use
// - only define one
// - each can be set independently (4 combos)
//#define USE_DACDS
//#define USE_DACT
#define USE_DAC

#if defined USE_DACDS
#  define DAC DacDS
#elif defined USE_DACT
#  define DAC DacT
#elif defined USE_DAC
#  define DAC Dac
#else
#  error "Must define one of USE_DAC*"
#endif


//-----------------------------------------------------------------


//-----------------------------------
// PCM data files
// - content at 8 & 16 bits for variety of samplerates
// - NOTE: for 16-bit, need to specify buffer type of uint16_t rather than int16_t since values are in hex
// - NOTE: be sure to specify const so that compiler doesn't try to put the data into (limited) RAM

const uint16_t viola4416Buf[] = {
#  include "data/viola.44.16.dat"
};
const uint8_t viola4408Buf[] = {
#  include "data/viola.44.08.dat"
};

const uint16_t viola3216Buf[] = {
#  include "data/viola.32.16.dat"
};
const uint8_t viola3208Buf[] = {
#  include "data/viola.32.08.dat"
};

const uint16_t viola2416Buf[] = {
#  include "data/viola.24.16.dat"
};
const uint8_t viola2408Buf[] = {
#  include "data/viola.24.08.dat"
};

const uint16_t viola2216Buf[] = {
#  include "data/viola.22.16.dat"
};
const uint8_t viola2208Buf[] = {
#  include "data/viola.22.08.dat"
};

const uint16_t viola1616Buf[] = {
#  include "data/viola.16.16.dat"
};
const uint8_t viola1608Buf[] = {
#  include "data/viola.16.08.dat"
};

const uint16_t viola1216Buf[] = {
#  include "data/viola.12.16.dat"
};
const uint8_t viola1208Buf[] = {
#  include "data/viola.12.08.dat"
};

const uint16_t viola0816Buf[] = {
#  include "data/viola.08.16.dat"
};
const uint8_t viola0808Buf[] = {
#  include "data/viola.08.08.dat"
};


const uint8_t * sampRateBufs08[] = {
    viola0808Buf,
    viola1208Buf,
    viola1608Buf,
    viola2208Buf,
    viola2408Buf,
    viola3208Buf,
    viola4408Buf
};

const int bufLens08[] = {
    sizeof(viola0808Buf)/1,
    sizeof(viola1208Buf)/1,
    sizeof(viola1608Buf)/1,
    sizeof(viola2208Buf)/1,
    sizeof(viola2408Buf)/1,
    sizeof(viola3208Buf)/1,
    sizeof(viola4408Buf)/1
};

const uint16_t * sampRateBufs16[] = {
    viola0816Buf,
    viola1216Buf,
    viola1616Buf,
    viola2216Buf,
    viola2416Buf,
    viola3216Buf,
    viola4416Buf
};

const int bufLens16[] = {
    sizeof(viola0816Buf)/2,
    sizeof(viola1216Buf)/2,
    sizeof(viola1616Buf)/2,
    sizeof(viola2216Buf)/2,
    sizeof(viola2416Buf)/2,
    sizeof(viola3216Buf)/2,
    sizeof(viola4416Buf)/2
};

const unsigned int sampRates[] = {
     8000,
    12000,
    16000,
    22050,
    24000,
    32000,
    44100
};

#define NUM_SAMPRATES (sizeof(sampRates)/sizeof(sampRates[0])) 
#define NUM_BUFS08    (sizeof(sampRateBufs08)/sizeof(sampRateBufs08[0]))
#define NUM_BUFS16    (sizeof(sampRateBufs16)/sizeof(sampRateBufs16[0]))

static_assert( NUM_SAMPRATES == NUM_BUFS08, "Number of items should match" );
static_assert( NUM_SAMPRATES == NUM_BUFS16, "Number of items should match" );


LoopTimer loopTimer;
Switch buttonSwitch(T0); // Touch0 = GPIO04

// This runs on powerup
// put your setup code here, to run once:
void setup()
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::log(__FILE__);
}

#if defined TEST_MODE_CYCLE_8B_SAMPRATES || defined TEST_MODE_CYCLE_16B_SAMPRATES 
const void * get_buf_params( unsigned int &sampleRate, unsigned int &bitDepth, unsigned int &bufLen )
{
    static int index = 9999;    // definitely more than the real number of buffers 
                                // - so after first increment we'll start at 0

    index++;
    if( index >= NUM_SAMPRATES )
        index = 0;

#  if defined TEST_MODE_CYCLE_8B_SAMPRATES
    const void *pBuf = sampRateBufs08[index];
    sampleRate = sampRates[index];
    bitDepth = 8;
    bufLen = bufLens08[index];
#  elif defined TEST_MODE_CYCLE_16B_SAMPRATES
    const void *pBuf = sampRateBufs16[index];
    sampleRate = sampRates[index];
    bitDepth = 16;
    bufLen = bufLens16[index];
#  endif

    return pBuf;
}
#endif

            
DAC * get_DAC(unsigned int sampleRate, bool looped, const void * pBuf, unsigned int bufLen, unsigned int bitDepth)
{
# if defined USE_DACDS
    return new DAC(sampleRate, looped, pBuf, bufLen, bitDepth);
# else
    // either USE_DAC or USE_DACT
    return new DAC(DAC1, sampleRate, looped, pBuf, bufLen, bitDepth);
# endif
}

DacVisualizer viz;


// Then this loop runs forever
// put your main code here, to run repeatedly:
void loop()
{
    // TODO: characterize loopTimer counts for various options: Dac, DacT, DacDS
    // - below values are out-of-date
    loopTimer.loop();   // typically 401400 calls/sec (LOOPED_MODE)
                        // typically 629300 calls/sec (non-LOOPED_MODE & not playing)
                        // w/ Ticker:
                        // typically 670000 calls/sec (LOOPED_MODE)
                        // unchanged 677000 calls/sec (non-LOOPED_MODE & not playing)
                        //  - guessing limited by loopTimer.Update() and the timer is still
                        //  firing

    buttonSwitch.loop();

    static const bool LOOPED = false;

    static DAC *dac = nullptr;
    static bool was_high = false;

    if(buttonSwitch.isHigh())
    {
        // kill the previous dac instance
        if( dac )
        {
            delete dac;
            dac = nullptr;
        }
        was_high = true;
    }
    else
    {
        // start next playback on switch transition from high->low
        if( was_high )
        {
            // get the buffer and its params
            unsigned int sampleRate;
            unsigned int bitDepth;
            unsigned int bufLen;
            const void *pBuf = get_buf_params( sampleRate, bitDepth, bufLen );  // implementation depends on TEST_MODE_*
            SerialLog::log( "bufLen: " + String(bufLen) );
            assert(bufLen > 1000);  // in case the above sizeof isn't doing what I hoped...

            // create new dac instance to play the buffer
            assert( dac == nullptr );
            dac = get_DAC(sampleRate, LOOPED, pBuf, bufLen, bitDepth);  // implementation depends on USE_DAC*
            viz.reset(dac);
            SerialLog::log( "Set samplerate/bitDepth: " + String(sampleRate) + "/" + String(bitDepth));

            was_high = false;
        }

#     if defined USE_DAC || defined USE_DACDS
        // non-ticker implementation requires pumping the ::loop() method
        if( dac )
            dac->loop();
#     endif
        if( dac )
            viz.loop();
    }

    //dac_ramp();
}

// vim: sw=4:ts=4
