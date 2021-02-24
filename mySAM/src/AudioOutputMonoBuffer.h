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
    AudioOutputMonoBuffer(int bufferSizeSamples);
    virtual ~AudioOutputMonoBuffer() override;
    virtual bool SetBitsPerSample(int bits) override;
    virtual bool SetChannels(int channels) override;
    virtual bool begin() override;
    virtual bool ConsumeSample(int16_t sample[2]) override;

  public:
    uint8_t *GetBuf();
    int GetBufUsed();
    unsigned int GetNumBufOverflows();
    void reset();

    // diagnostics
    int16_t minVal;
    int16_t maxVal;
    
  protected:
    typedef AudioOutput Super;
    uint8_t *samples;
    int buffSize;
    int writeIndex;
    unsigned int numOverflows;
};

#endif

