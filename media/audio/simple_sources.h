// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SIMPLE_SOURCES_H_
#define MEDIA_AUDIO_SIMPLE_SOURCES_H_

#include "media/audio/audio_output.h"

// An audio source that produces a pure sinusoidal tone. Each platform needs
// a slightly different implementation because it needs to handle the native
// audio buffer format.
class SineWaveAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  enum Format {
    FORMAT_8BIT_LINEAR_PCM,
    FORMAT_16BIT_LINEAR_PCM,
  };

  // |channels| is the number of audio channels, |freq| is the frequency in
  // hertz and it has to be less than half of the sampling frequency
  // |sample_freq| or else you will get aliasing.
  SineWaveAudioSource(Format format, int channels,
                      double freq, double sample_freq);
  virtual ~SineWaveAudioSource() {}

  // Implementation of AudioSourceCallback.
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 protected:
  Format format_;
  int channels_;
  double freq_;
  double sample_freq_;
};

#endif  // MEDIA_AUDIO_SIMPLE_SOURCES_H_
