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
uint8_t ConvertSampleTo8Bit(int16_t i16_val)
{
    // input value in range [-32768, 32767]
    i16_val >>= 8; // reduce range to [-128, 127]
    i16_val += 128; // range [0, 255]
    assert( i16_val >= 0 );
    assert( i16_val <= 255 );
    return (uint8_t)i16_val;
}

// Interface class to DAC functionality
// - initial use-case is for DacVisualizer
class IDac
{
    public:
        virtual bool IsPlaying() = 0;
        virtual unsigned int GetCurrentPos() = 0;
        virtual const void * GetDataBuffer() = 0;
        virtual unsigned int GetDataBufferLen() = 0;
        virtual unsigned int GetBitsPerSample() = 0;
        virtual unsigned int GetSamplerate() = 0;
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
    Dac(uint8_t dac_pin, unsigned int samplerate_Hz, bool looped=true, const void *buffer=nullptr, unsigned int buffer_len=0, unsigned int bits_per_sample=8)
        : looped_(looped)
        , bits_per_sample_(bits_per_sample)
        , done_(true)
    {
        // Verify DAC pin was specified
        assert( (dac_pin == DAC1) || (dac_pin == DAC2) );
        dac_pin_ = dac_pin;

        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        time_interval_ = 1000000 / samplerate_Hz;
        samplerate_ = samplerate_Hz;

        if( buffer != nullptr )
        {
            assert(buffer_len > 0 );

            SetBuffer(buffer, buffer_len, bits_per_sample);
            Restart();
        }
    }

    virtual ~Dac() {}

    // IDac Interface begin
    virtual bool IsPlaying() override
    {
        return (done_ == false);
    }
    virtual unsigned int GetCurrentPos() override
    {
        return buffer_pos_;
    }
    virtual const void * GetDataBuffer() override
    {
        return buffer_;
    }
    virtual unsigned int GetDataBufferLen() override
    {
        return buffer_len_;
    }
    virtual unsigned int GetBitsPerSample() override
    {
        return bits_per_sample_;
    }
    virtual unsigned int GetSamplerate() override
    {
        return samplerate_;
    }
    // IDac Interface end

    // Intended to restart one-shot (ie. non-looped) mode
    void Restart()
    {
        done_ = false;
        buffer_pos_ = 0;
        time_prev_toggle_ = 0;
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void SetBuffer(const void *buffer, unsigned int buffer_len, unsigned int bits_per_sample=8)
    {
        assert(buffer);
        buffer_ = buffer;
        buffer_len_ = buffer_len;
        bits_per_sample_ = bits_per_sample;
        if( bits_per_sample != 8 )
            SerialLog::Log( "Using 8-bit DAC to output data with bitdepth: " + String(bits_per_sample) );
        buffer_pos_ = 0;
    }

    void Loop()
    {
        if (done_)
            return;

        unsigned long time_now = micros();
        if((time_prev_toggle_ == 0) || (time_now >= time_prev_toggle_ + time_interval_))
        {
            uint8_t sample_val;
            if( bits_per_sample_ == 8 )
            {
                const uint8_t *buf = (uint8_t*)buffer_;
                sample_val = buf[buffer_pos_];
            }
            else
            {
                assert( bits_per_sample_ == 16 );
                const int16_t *buf = (int16_t*)buffer_;
                int16_t i16_val = buf[buffer_pos_];
                sample_val = ConvertSampleTo8Bit(i16_val);
            }

            dacWrite(dac_pin_, sample_val);
            buffer_pos_++;
            if(buffer_pos_ >= buffer_len_)
            {
                if( looped_ )
                {
                    buffer_pos_ = 0;
                }
                else
                {
                    done_ = true;
                    SerialLog::Log("DAC is done");
                }
            }

            time_prev_toggle_ = time_now;

            // TEST-CODE: report DAC outputs
#if 0
            {
                static const int REPORTING_INTERVAL = 10000;  // log every N DAC output periods
                static int count = 0;

                count++;
                if( count >= REPORTING_INTERVAL )
                {
                    SerialLog::Log(String(REPORTING_INTERVAL) + " DAC output intervals");
                    count = 0;
                }
            }
#endif
        }
    }

private:
    uint8_t dac_pin_;
    unsigned time_interval_;  // interval in micro secs
    unsigned long time_prev_toggle_;
    const void *buffer_;
    unsigned int buffer_len_;
    unsigned int buffer_pos_;  // public access to allow peeking
    bool looped_;
    unsigned int bits_per_sample_;
    unsigned int samplerate_;
    bool done_;
};

// Timer/Ticker-based 8-bit DAC implementation
// - ::_Loop() is periodically called automatically via the Ticker/Timer mechanism
// - ::loop() is provided as a dummy fcn for IDac API requirements but user doesn't need to call it
class DacT
{
public:
    DacT(uint8_t dac_pin, unsigned int samplerate_Hz, bool looped=true, const void *buffer=nullptr, unsigned int buffer_len=0, unsigned int bits_per_sample=8)
    : looped_(looped)
    , bits_per_sample_(bits_per_sample)
    , done_(true)
    {
        // Verify DAC pin was specified
        assert( (dac_pin == DAC1) || (dac_pin == DAC2) );
        dac_pin_ = dac_pin;

        if( buffer != nullptr )
        {
            assert(buffer_len > 0);

            SetBuffer(buffer, buffer_len, bits_per_sample);
            Restart();
        }

        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g.  8 kHz => 125 us output interval
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        uint32_t interval_us = 1000000 / samplerate_Hz;

        m_ticker.attach_us<DacT *>(interval_us, DacT::_Loop, this);
    }

    ~DacT()
    {
        m_ticker.detach();
    }

    // Intended to restart one-shot (ie. non-looped) mode
    void Restart()
    {
        done_ = false;
        buffer_pos_ = 0;
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void SetBuffer(const void *buffer, unsigned int buffer_len, unsigned int bits_per_sample=8)
    {
        assert(buffer);
        buffer_ = buffer;
        buffer_len_ = buffer_len;
        bits_per_sample_ = bits_per_sample;
        if( bits_per_sample != 8 )
            SerialLog::Log( "Using 8-bit DAC to output data with bitdepth: " + String(bits_per_sample) );
        buffer_pos_ = 0;
    }

    // Dummy implementation to fulfill IDac API requirements.  User doesn't need to call it
    void Loop()
    {
        return;
    }

private:
    static void _Loop(DacT *instance)
    {
        if (instance->done_)
            return;

        uint8_t sample_val;
        if( instance->bits_per_sample_ == 8 )
        {
            const uint8_t *buf = (uint8_t*)instance->buffer_;
            sample_val = buf[instance->buffer_pos_];
        }
        else
        {
            assert( instance->bits_per_sample_ == 16 );
            const int16_t *buf = (int16_t*)instance->buffer_;
            int16_t i16_val = buf[instance->buffer_pos_];
            sample_val = ConvertSampleTo8Bit(i16_val);
        }
        
        dacWrite(instance->dac_pin_, sample_val);
        instance->buffer_pos_++;
        if(instance->buffer_pos_ >= instance->buffer_len_)
        {
            if( instance->looped_ )
            {
                instance->buffer_pos_ = 0;
            }
            else
            {
                instance->done_ = true;
                SerialLog::Log("DAC is done");
            }
        }
    }


private:
    Ticker m_ticker;

    uint8_t dac_pin_;
    const void *buffer_;
    unsigned int buffer_len_;
    unsigned int buffer_pos_;  // public access to allow peeking
    bool looped_;
    unsigned int bits_per_sample_;
    bool done_;
};

// Polled Delta-Sigma DAC implementation
// Based on AudioOutputI2SNoDAC from: https://github.com/earlephilhower/ESP8266Audio
// TODO:
// - think can do a Ticker based implementation via additional ::loop() task that blocks on
// semaphore that is signaled by Ticker fcn.
// - this complication needed since the i2s_write() call crashes if called from the Ticker fcn
// (hence the need to run this polled)

// NOTE: finding that #includes of "../../DAC/include/Dac.h" needs to explicitly do the next include as well!
// - seems that VSCode or platform.io or Arduino does a limited scan to determine what libs it
// thinks it needs to incorporate...
#include <AudioOutputI2SNoDAC.h>
class DacDS
{
public:
    DacDS(unsigned int samplerate_Hz, bool looped=true, const void *buffer=nullptr, unsigned int buffer_len=0, unsigned int bits_per_sample=16)
    : samplerate_Hz_(samplerate_Hz)
    , bits_per_sample_(bits_per_sample)
    , looped_(looped)
    , done_(true)
    {
        i2s_output_ = new AudioOutputI2SNoDAC();    // TODO: see if compiler supports shared_ptr, I think it does...
        assert(i2s_output_);

        if( buffer != nullptr )
        {
            assert(buffer_len > 0 );

            SetBuffer(buffer, buffer_len, bits_per_sample);
            Restart();
        }

#if 0
        // best to pick outputFreq that divides into 1000 sans remainder
        // e.g.  8 kHz => 125 us output interval
        // e.g. 10 kHz => 100 us output interval
        // e.g. 20 kHz => 50 us output interval
        // e.g. 25 kHz => 40 us output interval
        uint32_t interval_us = 1000000 / samplerate_Hz;

//      m_ticker.attach_us<DacDS *>(interval_us, loop, this);
#endif
    }

    ~DacDS()
    {
#if 0
        m_ticker.detach();
#endif
        if( i2s_output_ )
        {
            i2s_output_->stop();
            delete i2s_output_;
        }
    }

    // Intended to restart one-shot (ie. non-looped) mode
    void Restart()
    {
        done_ = false;
        buffer_pos_ = 0;

        if( i2s_output_ )
        {
            bool ret;
            ret = i2s_output_->SetRate( samplerate_Hz_ );
            assert( ret );
            // 8->16 bit conversion handled in this class, so underlying AudioOutputI2SNoDAC always @ 16 bits
            ret = i2s_output_->SetBitsPerSample( 16 );
            assert( ret );
            ret = i2s_output_->SetChannels( 1 );
            assert( ret );
            ret = i2s_output_->begin();
            assert( ret );
        }
    }

    // Intended to change content for one-shot (ie. non-looped) mode
    void SetBuffer(const void *buffer, unsigned int buffer_len, unsigned int bits_per_sample=16)
    {
        assert(buffer);
        buffer_ = buffer;
        buffer_len_ = buffer_len;
        bits_per_sample_ = bits_per_sample;
        buffer_pos_ = 0;
    }

    void Loop()
    {
#if 0
        {
            static uint32_t call_count = 0;

            call_count++;
            if( (call_count%(samplerate_Hz_)) == 0 )
            {
                SerialLog::Log("DacDS::loop call count: " + String(call_count) );
            }
        }
#endif
        if (done_)
            return;

        int16_t sample_pair[2];
        if( bits_per_sample_ == 16 )
        {
            int16_t *buf = (int16_t *)buffer_;
            sample_pair[0] = buf[buffer_pos_];
            sample_pair[1] = sample_pair[0];
        }
        else
        {
            assert( bits_per_sample_ == 8 );
            uint8_t *buf = (uint8_t *)buffer_;
            sample_pair[0] = ((int16_t)buf[buffer_pos_] - 128) * 256;
            sample_pair[1] = sample_pair[0];
        }

#if 0
        // sample value stats gathering
        {
            static int16_t maxVal = -32768;
            static int16_t minVal =  32767;
            static unsigned count = 0;

            if( maxVal < sample_pair[0] )
                maxVal = sample_pair[0];
            if( minVal > sample_pair[0] )
                minVal = sample_pair[0];
            count++;
            if( count == 44100 )
            {
                SerialLog::Log("DacDS:: min/max Vals: " + String(minVal) + "/" + String(maxVal) );
                maxVal = -32768;
                minVal =  32767;
                count = 0;
            }
        }
#endif

        if( i2s_output_ )
        {
            // This call crashes if called from a Ticker fcn.
            bool ret = i2s_output_->ConsumeSample(sample_pair);

            if( ret )
            {
                buffer_pos_++;
                if(buffer_pos_ >= buffer_len_)
                {
                    if( looped_ )
                    {
                        buffer_pos_ = 0;
                    }
                    else
                    {
                        done_ = true;
                        i2s_output_->stop();
                        SerialLog::Log("DAC is done");
                    }
                }
            }
        }
    }


private:
#if 0
    Ticker m_ticker;
#endif
    AudioOutputI2SNoDAC *i2s_output_;

    unsigned samplerate_Hz_;
    unsigned int bits_per_sample_;
    const void *buffer_;
    unsigned int buffer_len_;
    unsigned int buffer_pos_;  // public access to allow peeking
    bool looped_;
    bool done_;
};
// vim: sw=4:ts=4
