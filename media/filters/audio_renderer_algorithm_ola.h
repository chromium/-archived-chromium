// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithmOLA [ARAO] is the pitch-preservation implementation of
// AudioRendererAlgorithmBase [ARAB]. For speeds greater than 1.0f, FillBuffer()
// consumes more input data than output data requested and crossfades samples
// to fill |buffer_out|. For speeds less than 1.0f, FillBuffer() consumers less
// input data than output data requested and draws overlapping samples from the
// input data to fill |buffer_out|. As ARAB is thread-unsafe, so is ARAO.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_

#include "media/filters/audio_renderer_algorithm_base.h"

namespace media {

class DataBuffer;

class AudioRendererAlgorithmOLA : public AudioRendererAlgorithmBase {
 public:
  AudioRendererAlgorithmOLA();
  virtual ~AudioRendererAlgorithmOLA();

  // AudioRendererAlgorithmBase implementation
  virtual size_t FillBuffer(DataBuffer* buffer_out);

  virtual void set_playback_rate(float new_rate);

 private:
  // Aligns |value| to a channel and sample boundary.
  void AlignToSampleBoundary(size_t* value);

  // Crossfades |samples| samples of |dest| with the data in |src|. Assumes
  // there is room in |dest| and enough data in |src|. Type is the datatype
  // of a data point in the waveform (i.e. uint8, int16, int32, etc). Also,
  // sizeof(one sample) == sizeof(Type) * channels.
  template <class Type>
  void Crossfade(int samples, const Type* src, Type* dest);

  // Members for ease of calculation in FillBuffer(). These members are based
  // on |playback_rate_|, but are stored seperately so they don't have to be
  // recalculated on every call to FillBuffer().
  size_t input_step_;
  size_t output_step_;

  // Length for crossfade in bytes.
  size_t crossfade_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmOLA);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_
