// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_ola.h"

#include <cmath>

#include "media/base/buffers.h"
#include "media/base/data_buffer.h"

namespace media {

// Default window size in bytes.
// TODO(kylep): base the window size in seconds, not bytes.
const size_t kDefaultWindowSize = 4096;

AudioRendererAlgorithmOLA::AudioRendererAlgorithmOLA()
    : input_step_(0),
      output_step_(0) {
}

AudioRendererAlgorithmOLA::~AudioRendererAlgorithmOLA() {
}

size_t AudioRendererAlgorithmOLA::FillBuffer(DataBuffer* buffer_out) {
  if (IsQueueEmpty())
    return 0;
  if (playback_rate() == 0.0f)
    return 0;

  // Grab info from |buffer_out| and handle the simple case of normal playback.
  size_t dest_remaining = buffer_out->GetDataSize();
  uint8* dest = buffer_out->GetWritableData(dest_remaining);
  size_t dest_written = 0;
  if (playback_rate() == 1.0f) {
    if (QueueSize() < dest_remaining)
      dest_written = CopyFromInput(dest, QueueSize());
    else
      dest_written = CopyFromInput(dest, dest_remaining);
    AdvanceInputPosition(dest_written);
    return dest_written;
  }

  // For other playback rates, OLA with crossfade!
  // TODO(kylep): Limit the rates to reasonable values. We may want to do this
  // on the UI side or in set_playback_rate().
  while (dest_remaining >= output_step_ + crossfade_size_) {
    // If we don't have enough data to completely finish this loop, quit.
    if (QueueSize() < kDefaultWindowSize)
      break;

    // Copy bulk of data to output (including some to crossfade to the next
    // copy), then add to our running sum of written data and subtract from
    // our tally of remaing requested.
    size_t copied = CopyFromInput(dest, output_step_ + crossfade_size_);
    dest_written += copied;
    dest_remaining -= copied;

    // Advance pointers for crossfade.
    dest += output_step_;
    AdvanceInputPosition(input_step_);

    // Prepare intermediate buffer.
    size_t crossfade_size;
    scoped_array<uint8> src(new uint8[crossfade_size_]);
    crossfade_size = CopyFromInput(src.get(), crossfade_size_);

    // Calculate number of samples to crossfade, then do so.
    int samples = static_cast<int>(crossfade_size / sample_bytes()
        / channels());
    switch (sample_bytes()) {
      case 4:
        Crossfade(samples,
            reinterpret_cast<const int32*>(src.get()),
            reinterpret_cast<int32*>(dest));
        break;
      case 2:
        Crossfade(samples,
            reinterpret_cast<const int16*>(src.get()),
            reinterpret_cast<int16*>(dest));
        break;
      case 1:
        Crossfade(samples, src.get(), dest);
        break;
      default:
        NOTREACHED() << "Unsupported audio bit depth sent to OLA algorithm";
    }

    // Advance pointers again.
    AdvanceInputPosition(crossfade_size);
    dest += crossfade_size;
  }
  return dest_written;
}

void AudioRendererAlgorithmOLA::set_playback_rate(float new_rate) {
  AudioRendererAlgorithmBase::set_playback_rate(new_rate);

  // Adjusting step sizes to accomodate requested playback rate.
  if (playback_rate() > 1.0f) {
    input_step_ = kDefaultWindowSize;
    output_step_ = static_cast<size_t>(ceil(
        static_cast<float>(kDefaultWindowSize / playback_rate())));
  } else {
    input_step_ = static_cast<size_t>(ceil(
        static_cast<float>(kDefaultWindowSize * playback_rate())));
    output_step_ = kDefaultWindowSize;
  }
  AlignToSampleBoundary(&input_step_);
  AlignToSampleBoundary(&output_step_);

  // Calculate length for crossfading.
  crossfade_size_ = kDefaultWindowSize / 10;
  AlignToSampleBoundary(&crossfade_size_);

  // To keep true to playback rate, modify the steps.
  input_step_ -= crossfade_size_;
  output_step_ -= crossfade_size_;
}

void AudioRendererAlgorithmOLA::AlignToSampleBoundary(size_t* value) {
  (*value) -= ((*value) % (channels() * sample_bytes()));
}

template <class Type>
void AudioRendererAlgorithmOLA::Crossfade(int samples,
                                          const Type* src,
                                          Type* dest) {
  Type* dest_end = dest + samples * channels();
  const Type* src_end = src + samples * channels();
  for (int i = 0; i < samples; ++i) {
    double x_ratio = static_cast<double>(i) / static_cast<double>(samples);
    for (int j = 0; j < channels(); ++j) {
      DCHECK(dest < dest_end);
      DCHECK(src < src_end);
      (*dest) = static_cast<Type>((*dest) * (1.0 - x_ratio) +
                                  (*src) * x_ratio);
      ++src;
      ++dest;
    }
  }
}

}  // namespace media
