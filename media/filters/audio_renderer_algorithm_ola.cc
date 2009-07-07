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
    : data_offset_(0),
      input_step_(0),
      output_step_(0) {
}

AudioRendererAlgorithmOLA::~AudioRendererAlgorithmOLA() {
}

size_t AudioRendererAlgorithmOLA::FillBuffer(DataBuffer* buffer_out) {
  if (IsInputFinished())
    return 0;
  if (playback_rate() == 0.0f)
    return 0;

  // Grab info from |buffer_out| and handle the simple case of normal playback.
  size_t dest_remaining = buffer_out->GetDataSize();
  uint8* dest = buffer_out->GetWritableData(dest_remaining);
  size_t dest_written = 0;
  if (playback_rate() == 1.0f) {
    dest_written = CopyInput(dest, dest_remaining);
    AdvanceInput(dest_written);
    return dest_written;
  }

  // For other playback rates, OLA with crossfade!
  // TODO(kylep): Limit the rates to reasonable values. We may want to do this
  // on the UI side or in set_playback_rate().
  while (dest_remaining >= output_step_ + crossfade_size_) {
    // Copy bulk of data to output (including some to crossfade to the next
    // copy), then add to our running sum of written data and subtract from
    // our tally of remaing requested.
    size_t copied = CopyInput(dest, output_step_ + crossfade_size_);
    dest_written += copied;
    dest_remaining -= copied;

    // Advance pointers for crossfade.
    dest += output_step_;
    AdvanceInput(input_step_);

    // Prepare intermediate buffer.
    size_t crossfade_size;
    scoped_array<uint8> src(new uint8[crossfade_size_]);
    crossfade_size = CopyInput(src.get(), crossfade_size_);

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
    AdvanceInput(crossfade_size_);
    dest += crossfade_size_;
  }
  return dest_written;
}

void AudioRendererAlgorithmOLA::FlushBuffers() {
  AudioRendererAlgorithmBase::FlushBuffers();
  saved_buf_ = NULL;
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

void AudioRendererAlgorithmOLA::AdvanceInput(size_t bytes) {
  if (IsInputFinished())
    return;

  DCHECK(saved_buf_) << "Did you forget to call CopyInput()?";

  // Calculate number of usable bytes in |saved_buf_|.
  size_t saved_buf_remaining = saved_buf_->GetDataSize() - data_offset_;

  // If there is enough data in |saved_buf_| to advance into it, do so.
  // Otherwise, advance into the queue.
  if (saved_buf_remaining > bytes) {
    data_offset_ += bytes;
  } else {
    if (!IsQueueEmpty()) {
      saved_buf_ = FrontQueue();
      PopFrontQueue();
    } else {
      saved_buf_ = NULL;
    }
    // TODO(kylep): Make this function loop to eliminate the DCHECK.
    DCHECK_GE(bytes, saved_buf_remaining);

    data_offset_ = bytes - saved_buf_remaining;
  }
}

void AudioRendererAlgorithmOLA::AlignToSampleBoundary(size_t* value) {
  (*value) -= ((*value) % (channels() * sample_bytes()));
}

// TODO(kylep): Make this function loop to satisfy requests better.
size_t AudioRendererAlgorithmOLA::CopyInput(uint8* dest, size_t length) {
  if (IsInputFinished())
    return 0;

  // Lazy initialization.
  if (!saved_buf_) {
    saved_buf_ = FrontQueue();
    PopFrontQueue();
  }

  size_t dest_written = 0;
  size_t data_length = saved_buf_->GetDataSize() - data_offset_;

  // Prevent writing past end of the buffer.
  if (data_length > length)
    data_length = length;
  memcpy(dest, saved_buf_->GetData() + data_offset_, data_length);

  dest += data_length;
  length -= data_length;
  dest_written += data_length;

  if (length > 0) {
    // We should have enough data in the next buffer so long as the
    // queue is not empty.
    if (IsQueueEmpty())
      return dest_written;
    DCHECK_LE(length, FrontQueue()->GetDataSize());

    memcpy(dest, FrontQueue()->GetData(), length);
    dest_written += length;
  }

  return dest_written;
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

bool AudioRendererAlgorithmOLA::IsInputFinished() {
  return !saved_buf_ && IsQueueEmpty();
}

}  // namespace media
