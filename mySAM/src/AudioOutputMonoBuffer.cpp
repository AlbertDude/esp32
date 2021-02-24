/*
  AudioOutputMonoBuffer 
  - memory buffer output sink for ESP8266SAM
*/

#include <Arduino.h>
#include "AudioOutputMonoBuffer.h"


AudioOutputMonoBuffer::AudioOutputMonoBuffer(int buffSizeSamples)
{
  buffSize = buffSizeSamples;
  samples = (uint8_t*)malloc(sizeof(uint8_t) * buffSize);
  assert(samples);
  reset();
}

AudioOutputMonoBuffer::~AudioOutputMonoBuffer()
{
  free(samples);
}

bool AudioOutputMonoBuffer::SetBitsPerSample(int bits)
{
  assert(bits == 8);    // TODO: extend support to 16 bits
  return Super::SetBitsPerSample(bits);
}

bool AudioOutputMonoBuffer::SetChannels(int channels)
{
  assert(channels == 1);    // Only supports 1 channel
  return Super::SetChannels(channels);
}

bool AudioOutputMonoBuffer::begin()
{
  reset();
  return Super::begin();
}

void AudioOutputMonoBuffer::reset()
{
  numOverflows = 0;
  writeIndex = 0;
  maxVal = -32767;
  minVal =  32767;
}

uint8_t *AudioOutputMonoBuffer::GetBuf()
{
    return samples;
}

int AudioOutputMonoBuffer::GetBufUsed()
{
    return writeIndex;
}

unsigned int AudioOutputMonoBuffer::GetNumBufOverflows()
{
    return numOverflows;
}

bool AudioOutputMonoBuffer::ConsumeSample(int16_t sample[2])
{
  // is buffer full?
  if (writeIndex == buffSize) 
  {
    numOverflows++;
    return true;
  }

  // diagnostics: track sample extrema
  if(sample[LEFTCHANNEL] < minVal) 
      minVal = sample[LEFTCHANNEL];
  if(sample[LEFTCHANNEL] > maxVal) 
      maxVal = sample[LEFTCHANNEL];

  // clamp to 8-bit range
  uint8_t samp8;
  if( sample[LEFTCHANNEL] < 0 )
      samp8 = 0;
  else if( sample[LEFTCHANNEL] > 255 )
      samp8 = 255;
  else
      samp8 = sample[LEFTCHANNEL];

  samples[writeIndex] = samp8;
  writeIndex++;
  return true;
}


