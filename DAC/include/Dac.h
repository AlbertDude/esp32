/*
 * DAC Audio Output implementations
 * - either via 8-bit DAC outputs "DAC1" or "DAC2"
 * - or via a 1-bit delta-sigma output to I2SOut

 
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

#pragma once

#include "../../SerialLog/include/SerialLog.h"
#include "../../Ticker/include/Ticker.h"
#include <assert.h>

// Helpers

// Convert int16_t sample to uint8_t sample
uint8_t convert16to8(int16_t int16_val)
{
    // input value in range [-32768, 32767]
    int16_val >>= 8; // reduce range to [-128, 127]
    int16_val += 128; // range [0, 255]
    assert( int16_val >= 0 );
    assert( int16_val <= 255 );
    return (uint8_t)int16_val;
}

// Interface class to DAC functionality
// - initial use-case is for DacVisualizer
class IDac
{
    public:
        virtual bool isPlaying() = 0;
        virtual unsigned int getCurrentPos() = 0;
        virtual const void * getDataBuffer() = 0;
        virtual unsigned int getDataBufferLen() = 0;
        virtual unsigned int getBitsPerSample() = 0;
        virtual unsigned int getSamplerate() = 0;
};

// Polled 8-bit DAC implementation
// - user needs to call ::loop() periodically at a rate faster than the output sampling rate.
// - mainly for illustrative purposes, should use DacT for Timer-driven implementation which is:
//  - lighter on CPU load
// - ALSO NOTE:
//  - this implementation uses micros() which will roll over every 70 mins or so, so beware of random glitch
//  - another reason to use DacT instead
class Dac : public IDac
{
public:
    Dac(uint8_t dacPin, unsigned int samplingFreqHz, bool loop=true, const void *buffer=nullptr, unsigned int bufLen=0, unsigned int bitsPerSample=8)
    : m_loop(loop)
    , m_bitsPerSample(bitsPerSample)
    , m_done(true)
    {
        // Verify DAC pin was specified
        assert((dacPin==DAC1) || (dacPin==DAC2));
        m_dacPin = dacPin;

        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        m_interval = 1000000 / samplingFreqHz;
        m_samplerate = samplingFreqHz;

        if( buffer != nullptr )
        {
            assert(bufLen > 0 );

            setBuffer(buffer, bufLen, bitsPerSample);
            restart();
        }
    }

    virtual ~Dac() {}

    // IDac Interface begin
    virtual bool isPlaying() override
    {
        return (m_done == false);
    }
    virtual unsigned int getCurrentPos() override
    {
        return m_bufIndex;
    }
    virtual const void * getDataBuffer() override
    {
        return m_buffer;
    }
    virtual unsigned int getDataBufferLen() override
    {
        return m_bufLen;
    }
    virtual unsigned int getBitsPerSample() override
    {
        return m_bitsPerSample;
    }
    virtual unsigned int getSamplerate() override
    {
        return m_samplerate;
    }
    // IDac Interface end

    // Intended to restart one-shot (ie. non-looped) mode
    void restart()
    {
        m_done = false;
        m_bufIndex = 0;
        m_prevToggle = 0;
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void setBuffer(const void *buffer, unsigned int bufLen, unsigned int bitsPerSample=8)
    {
        assert(buffer);
        m_buffer = buffer;
        m_bufLen = bufLen;
        m_bitsPerSample = bitsPerSample;
        if( bitsPerSample != 8 )
            SerialLog::log( "Using 8-bit DAC to output data with bitdepth: " + String(bitsPerSample) );
        m_bufIndex = 0;
    }

    void loop()
    {
        if (m_done)
            return;

        unsigned long now = micros();
        if((m_prevToggle == 0) || (now >= m_prevToggle + m_interval))
        {
            uint8_t sample_val;
            if( m_bitsPerSample == 8 )
            {
                const uint8_t *pBuf = (uint8_t*)m_buffer;
                sample_val = pBuf[m_bufIndex];
            }
            else
            {
                assert( m_bitsPerSample == 16 );
                const int16_t *pBuf = (int16_t*)m_buffer;
                int16_t int16_val = pBuf[m_bufIndex];
                sample_val = convert16to8(int16_val);
            }

            dacWrite(m_dacPin, sample_val);
            m_bufIndex++;
            if(m_bufIndex >= m_bufLen)
            {
                if( m_loop )
                {
                    m_bufIndex = 0;
                }
                else
                {
                    m_done = true;
                    SerialLog::log("DAC is done");
                }
            }

            m_prevToggle = now;

            // TEST-CODE: report DAC outputs
#if 0
            {
                static const int REPORTING_INTERVAL = 10000;  // log every N DAC output periods
                static int count = 0;

                count++;
                if( count >= REPORTING_INTERVAL )
                {
                    SerialLog::log(String(REPORTING_INTERVAL) + " DAC output intervals");
                    count = 0;
                }
            }
#endif
        }
    }


    unsigned int m_bufIndex;  // public access to allow peeking

private:
    unsigned m_interval;  // interval in micro secs
    uint8_t m_dacPin;
    unsigned long m_prevToggle;
    const void *m_buffer;
    unsigned int m_bufLen;
    bool m_loop;
    unsigned int m_bitsPerSample;
    unsigned int m_samplerate;
    bool m_done;
};

// Timer/Ticker-based 8-bit DAC implementation
// - ::_loop() is periodically called automatically via the Ticker/Timer mechanism
// - ::loop() is provided as a dummy fcn for IDac API requirements but user doesn't need to call it
class DacT
{
public:
    DacT(uint8_t dacPin, unsigned int samplingFreqHz, bool loop=true, const void *buffer=nullptr, unsigned int bufLen=0, unsigned int bitsPerSample=8)
    : m_loop(loop)
    , m_bitsPerSample(bitsPerSample)
    , m_done(true)
    {
        // Verify DAC pin was specified
        assert((dacPin==DAC1) || (dacPin==DAC2));
        m_dacPin = dacPin;

        if( buffer != nullptr )
        {
            assert(bufLen > 0 );

            setBuffer(buffer, bufLen, bitsPerSample);
            restart();
        }

        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g.  8 kHz => 125 us output interval
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        uint32_t interval_us = 1000000 / samplingFreqHz;

        m_ticker.attach_us<DacT *>(interval_us, DacT::_loop, this);
    }

    ~DacT()
    {
        m_ticker.detach();
    }

    // Intended to restart one-shot (ie. non-looped) mode
    void restart()
    {
        m_done = false;
        m_bufIndex = 0;
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void setBuffer(const void *buffer, unsigned int bufLen, unsigned int bitsPerSample=8)
    {
        assert(buffer);
        m_buffer = buffer;
        m_bufLen = bufLen;
        m_bitsPerSample = bitsPerSample;
        if( bitsPerSample != 8 )
            SerialLog::log( "Using 8-bit DAC to output data with bitdepth: " + String(bitsPerSample) );
        m_bufIndex = 0;
    }

    // Dummy implementation to fulfill IDac API requirements.  User doesn't need to call it
    void loop()
    {
        return;
    }

    unsigned int m_bufIndex;  // public access to allow peeking

private:
    static void _loop(DacT *instance)
    {
        if (instance->m_done)
            return;

        uint8_t sample_val;
        if( instance->m_bitsPerSample == 8 )
        {
            const uint8_t *pBuf = (uint8_t*)instance->m_buffer;
            sample_val = pBuf[instance->m_bufIndex];
        }
        else
        {
            assert( instance->m_bitsPerSample == 16 );
            const int16_t *pBuf = (int16_t*)instance->m_buffer;
            int16_t int16_val = pBuf[instance->m_bufIndex];
            sample_val = convert16to8(int16_val);
        }
        
        dacWrite(instance->m_dacPin, sample_val);
        instance->m_bufIndex++;
        if(instance->m_bufIndex >= instance->m_bufLen)
        {
            if( instance->m_loop )
            {
                instance->m_bufIndex = 0;
            }
            else
            {
                instance->m_done = true;
                SerialLog::log("DAC is done");
            }
        }
    }


private:
    Ticker m_ticker;

    uint8_t m_dacPin;
    const void *m_buffer;
    unsigned int m_bufLen;
    bool m_loop;
    unsigned int m_bitsPerSample;
    bool m_done;
};

// Polled Delta-Sigma DAC implementation
// Based on AudioOutputI2SNoDAC from: https://github.com/earlephilhower/ESP8266Audio
// TODO:
// - think can do a Ticker based implementation via additional ::loop() task that blocks on
// semaphore that is signaled by Ticker fcn.
// - this complication needed since the i2s_write() call crashes if called from the Ticker fcn
// (hence the need to run this polled)

// NOTE: finding that #includers of "../../DAC/include/Dac.h" needs to explicitly do the next include as well!
// - seems that VSCode or platform.io or Arduino does a limited scan to determine what libs it
// thinks it needs to incorporate...
#include <AudioOutputI2SNoDAC.h>
class DacDS
{
public:
    DacDS(unsigned int samplingFreqHz, bool loop=true, const void *buffer=nullptr, unsigned int bufLen=0, unsigned int bitsPerSample=16)
    : m_samplingFreqHz(samplingFreqHz)
    , m_bitsPerSample(bitsPerSample)
    , m_loop(loop)
    , m_done(true)
    {
        m_I2SOutput = new AudioOutputI2SNoDAC();    // TODO: see if compiler supports shared_ptr, I think it does...
        assert(m_I2SOutput);

        if( buffer != nullptr )
        {
            assert(bufLen > 0 );

            setBuffer(buffer, bufLen, bitsPerSample);
            restart();
        }

#if 0
        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g.  8 kHz => 125 us output interval
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        uint32_t interval_us = 1000000 / samplingFreqHz;

//      m_ticker.attach_us<DacDS *>(interval_us, loop, this);
#endif
    }

    ~DacDS()
    {
#if 0
        m_ticker.detach();
#endif
        if( m_I2SOutput )
        {
            m_I2SOutput->stop();
            delete m_I2SOutput;
        }
    }

    // Intended to restart one-shot (ie. non-looped) mode
    void restart()
    {
        m_done = false;
        m_bufIndex = 0;

        if( m_I2SOutput )
        {
            bool ret;
            ret = m_I2SOutput->SetRate( m_samplingFreqHz );
            assert( ret );
            // 8->16 bit conversion handled in this class, so underlying AudioOutputI2SNoDAC always @ 16 bits
            ret = m_I2SOutput->SetBitsPerSample( 16 );
            assert( ret );
            ret = m_I2SOutput->SetChannels( 1 );
            assert( ret );
            ret = m_I2SOutput->begin();
            assert( ret );
        }
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void setBuffer(const void *buffer, unsigned int bufLen, unsigned int bitsPerSample=16)
    {
        assert(buffer);
        m_buffer = buffer;
        m_bufLen = bufLen;
        m_bitsPerSample = bitsPerSample;
        m_bufIndex = 0;
    }
    unsigned int m_bufIndex;  // public access to allow peeking

    void loop()
    {
#if 0
        {
            static uint32_t call_count = 0;

            call_count++;
            if( (call_count%(m_samplingFreqHz)) == 0 )
            {
                SerialLog::log("DacDS::loop call count: " + String(call_count) );
            }
        }
#endif
        if (m_done)
            return;

        int16_t samplePair[2];
        if( m_bitsPerSample == 16 )
        {
            int16_t *pBuf = (int16_t *)m_buffer;
            samplePair[0] = pBuf[m_bufIndex];
            samplePair[1] = samplePair[0];
        }
        else
        {
            assert( m_bitsPerSample == 8 );
            uint8_t *pBuf = (uint8_t *)m_buffer;
            samplePair[0] = ((int16_t)pBuf[m_bufIndex] - 128) * 256;
            samplePair[1] = samplePair[0];
        }

#if 0
        // sample value stats gathering
        {
            static int16_t maxVal = -32768;
            static int16_t minVal =  32767;
            static unsigned count = 0;

            if( maxVal < samplePair[0] )
                maxVal = samplePair[0];
            if( minVal > samplePair[0] )
                minVal = samplePair[0];
            count++;
            if( count == 44100 )
            {
                SerialLog::log("DacDS:: min/max Vals: " + String(minVal) + "/" + String(maxVal) );
                maxVal = -32768;
                minVal =  32767;
                count = 0;
            }
        }
#endif

        if( m_I2SOutput )
        {
            // This call crashes if called from a Ticker fcn.
            bool ret = m_I2SOutput->ConsumeSample(samplePair);

            if( ret )
            {
                m_bufIndex++;
                if(m_bufIndex >= m_bufLen)
                {
                    if( m_loop )
                    {
                        m_bufIndex = 0;
                    }
                    else
                    {
                        m_done = true;
                        m_I2SOutput->stop();
                        SerialLog::log("DAC is done");
                    }
                }
            }
        }
    }


private:
#if 0
    Ticker m_ticker;
#endif
    AudioOutputI2SNoDAC *m_I2SOutput;

    unsigned m_samplingFreqHz;
    unsigned int m_bitsPerSample;
    const void *m_buffer;
    unsigned int m_bufLen;
    bool m_loop;
    bool m_done;
};
// vim: sw=4:ts=4
