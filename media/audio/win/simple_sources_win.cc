// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"

SineWaveAudioSource::SineWaveAudioSource(Format format, int channels,
                                         double freq, double sample_freq)
    : format_(format),
      channels_(channels),
      freq_(freq),
      sample_freq_(sample_freq) {
  // TODO(cpu): support other formats.
  DCHECK((format_ == FORMAT_16BIT_LINEAR_PCM) && (channels_ == 1));
}

// The implementation could be more efficient if a lookup table is constructed
// but it is efficient enough for our simple needs.
size_t SineWaveAudioSource::OnMoreData(AudioOutputStream* stream,
                                       void* dest, size_t max_size) {
  const double kTwoPi = 2.0 * 3.141592653589;
  double f = freq_ / sample_freq_;
  int16* sin_tbl = reinterpret_cast<int16*>(dest);
  size_t len = max_size / sizeof(int16);
  // The table is filled with s(t) = 32768*sin(2PI*f*t).
  for (size_t ix = 0; ix != len; ++ix) {
    double th = kTwoPi * ix * f;
    sin_tbl[ix] = static_cast<int16>((1 << 15) * sin(th));
  }
  return max_size;
}

void SineWaveAudioSource::OnClose(AudioOutputStream* stream) {
}

void SineWaveAudioSource::OnError(AudioOutputStream* stream, int code) {
  NOTREACHED();
}
