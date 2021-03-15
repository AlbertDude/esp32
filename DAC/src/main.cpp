
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
void DacRamp()
{
    const static uint8_t kOutputValueMin = 56;  // corresponds to output voltage ~0.75V
    static uint8_t output_value = kOutputValueMin;
    static unsigned long time_prev_toggle = 0;
    static unsigned long time_interval = 20; // increment/decrement interval
    //static unsigned long time_interval = 7000; // for characterization
    static bool is_incrementing = true;

    unsigned long time_now = millis();
    if((time_prev_toggle == 0) || (time_now >= time_prev_toggle + time_interval))
    {
        dacWrite(DAC1, output_value);
        //SerialLog::Log("DAC value: " + String(output_value));
        if(is_incrementing)
        {
            output_value++;
            if(output_value==255){
                is_incrementing = false;
            }
        }
        else
        {
            output_value--;
            if(output_value==kOutputValueMin)
            {
                is_incrementing = true;
            }
        }
        time_prev_toggle = time_now;
    }
}


// Pick test mode
// - only define one
#define TEST_MODE_CYCLE_8B_SAMPRATES
//#define TEST_MODE_CYCLE_16B_SAMPRATES

// TODO: create this test mode also
//#define TEST_MODE_CYCLE_BITDEPTHS


// Pick DAC variant to use
// - only define one
// - use of Dac or DacT will default to DAC1 pin output

//#define DAC DacDS
//#define DAC DacT
#define DAC Dac


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


const uint8_t * samplerate_bufs_08[] = {
    viola0808Buf,
    viola1208Buf,
    viola1608Buf,
    viola2208Buf,
    viola2408Buf,
    viola3208Buf,
    viola4408Buf
};

const int buf_lens_08[] = {
    sizeof(viola0808Buf)/1,
    sizeof(viola1208Buf)/1,
    sizeof(viola1608Buf)/1,
    sizeof(viola2208Buf)/1,
    sizeof(viola2408Buf)/1,
    sizeof(viola3208Buf)/1,
    sizeof(viola4408Buf)/1
};

const uint16_t * samplerate_bufs_16[] = {
    viola0816Buf,
    viola1216Buf,
    viola1616Buf,
    viola2216Buf,
    viola2416Buf,
    viola3216Buf,
    viola4416Buf
};

const int buf_lens_16[] = {
    sizeof(viola0816Buf)/2,
    sizeof(viola1216Buf)/2,
    sizeof(viola1616Buf)/2,
    sizeof(viola2216Buf)/2,
    sizeof(viola2416Buf)/2,
    sizeof(viola3216Buf)/2,
    sizeof(viola4416Buf)/2
};

const unsigned int samplerates[] = {
     8000,
    12000,
    16000,
    22050,
    24000,
    32000,
    44100
};

#define NUM_SAMPRATES (sizeof(samplerates)/sizeof(samplerates[0])) 
#define NUM_BUFS08    (sizeof(samplerate_bufs_08)/sizeof(samplerate_bufs_08[0]))
#define NUM_BUFS16    (sizeof(samplerate_bufs_16)/sizeof(samplerate_bufs_16[0]))

static_assert( NUM_SAMPRATES == NUM_BUFS08, "Number of items should match" );
static_assert( NUM_SAMPRATES == NUM_BUFS16, "Number of items should match" );


LoopTimer loop_timer;
Switch button_switch(T0); // Touch0 = GPIO04

// This runs on powerup
// put your setup code here, to run once:
void setup()
{
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
}

#if defined TEST_MODE_CYCLE_8B_SAMPRATES || defined TEST_MODE_CYCLE_16B_SAMPRATES 
const void * GetBufParams( unsigned int &samplerate, unsigned int &bit_depth, unsigned int &buf_len )
{
    static int index = 9999;    // definitely more than the real number of buffers 
                                // - so after first increment we'll start at 0

    index++;
    if( index >= NUM_SAMPRATES )
        index = 0;

#  if defined TEST_MODE_CYCLE_8B_SAMPRATES
    const void *buf = samplerate_bufs_08[index];
    samplerate = samplerates[index];
    bit_depth = 8;
    buf_len = buf_lens_08[index];
#  elif defined TEST_MODE_CYCLE_16B_SAMPRATES
    const void *buf = samplerate_bufs_16[index];
    samplerate = samplerates[index];
    bit_depth = 16;
    buf_len = buf_lens_16[index];
#  endif

    return buf;
}
#endif

            
DAC * GetDac(unsigned int samplerate, bool looped, const void * buf, unsigned int buf_len, unsigned int bit_depth)
{
    return new DAC(samplerate, looped, buf, buf_len, bit_depth);
}

DacVisualizer viz;


// Then this loop runs forever
// put your main code here, to run repeatedly:
void loop()
{
    // TODO: characterize loopTimer counts for various options: Dac, DacT, DacDS
    // - below values are out-of-date
    loop_timer.Loop();  // typically 401400 calls/sec (LOOPED_MODE)
                        // typically 629300 calls/sec (non-LOOPED_MODE & not playing)
                        // w/ Ticker:
                        // typically 670000 calls/sec (LOOPED_MODE)
                        // unchanged 677000 calls/sec (non-LOOPED_MODE & not playing)
                        //  - guessing limited by loopTimer.Update() and the timer is still
                        //  firing

    button_switch.Loop();

    static const bool kLooped = false;

    static DAC *dac = nullptr;
    static bool was_high = false;

    if(button_switch.IsHigh())
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
            unsigned int samplerate;
            unsigned int bit_depth;
            unsigned int buf_len;
            const void *buf = GetBufParams( samplerate, bit_depth, buf_len );  // implementation depends on TEST_MODE_*
            SerialLog::Log( "buf_len: " + String(buf_len) );
            assert(buf_len > 1000);  // in case the above sizeof isn't doing what I hoped...

            // create new dac instance to play the buffer
            assert( dac == nullptr );
            dac = GetDac(samplerate, kLooped, buf, buf_len, bit_depth);  // implementation depends on USE_DAC*
            viz.Reset(dac);
            SerialLog::Log( "Set samplerate/bit_depth: " + String(samplerate) + "/" + String(bit_depth));

            was_high = false;
        }

        if( dac )
        {
            dac->Loop();
            viz.Loop();
        }
    }

    //DacRamp();
}

// vim: sw=4:ts=4
