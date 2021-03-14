/*
  AudioOutputMonoBuffer
  - memory buffer output sink for ESP8266SAM
*/

#ifndef _AUDIOOUTPUTMONOBUFFER_H
#define _AUDIOOUTPUTMONOBUFFER_H

#include "AudioOutput.h"


class AudioOutputMonoBuffer : public AudioOutput
{
  public:
    AudioOutputMonoBuffer(int buffer_len)
    {
      buffer_len_ = buffer_len;
      buffer_ = (uint8_t*)malloc(sizeof(uint8_t) * buffer_len_);
      assert(buffer_);
      Reset();
    }

    virtual ~AudioOutputMonoBuffer() override
    {
      free(buffer_);
    }

    virtual bool SetBitsPerSample(int bits) override
    {
      assert(bits == 8);    // TODO: extend support to 16 bits
      return Super::SetBitsPerSample(bits);
    }

    virtual bool SetChannels(int channels) override
    {
      assert(channels == 1);    // Only supports 1 channel
      return Super::SetChannels(channels);
    }

    virtual bool begin() override
    {
      Reset();
      return Super::begin();
    }

    virtual bool ConsumeSample(int16_t sample[2]) override
    {
      // is buffer full?
      if (write_index_ == buffer_len_) 
      {
        num_overflows_++;
        return true;
      }

      // diagnostics: track sample extrema
      if(sample[LEFTCHANNEL] < min_val_) 
          min_val_ = sample[LEFTCHANNEL];
      if(sample[LEFTCHANNEL] > max_val_) 
          max_val_ = sample[LEFTCHANNEL];

      // clamp to 8-bit range
      uint8_t samp8;
      if( sample[LEFTCHANNEL] < 0 )
          samp8 = 0;
      else if( sample[LEFTCHANNEL] > 255 )
          samp8 = 255;
      else
          samp8 = sample[LEFTCHANNEL];

      buffer_[write_index_] = samp8;
      write_index_++;
      return true;
    }

  public:
    uint8_t *GetBuf()
    {
        return buffer_;
    }

    int GetBufUsed()
    {
        return write_index_;
    }

    unsigned int GetNumBufOverflows()
    {
        return num_overflows_;
    }

    void Reset()
    {
      num_overflows_ = 0;
      write_index_ = 0;
      max_val_ = -32767;
      min_val_ =  32767;
    }

    // diagnostics
    int16_t min_val_;
    int16_t max_val_;
    
  protected:
    typedef AudioOutput Super;
    uint8_t *buffer_;
    int buffer_len_;
    int write_index_;
    unsigned int num_overflows_;
};

#endif

