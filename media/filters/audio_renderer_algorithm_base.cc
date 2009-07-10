// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

#include "media/base/buffers.h"

namespace media {

// The size in bytes we try to maintain for the |queue_|. Previous usage
// maintained a deque of 16 Buffers, each of size 4Kb. This worked well, so we
// maintain this number of bytes (16 * 4096).
const size_t kDefaultMaxQueueSizeInBytes = 65536;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      sample_bytes_(0),
      playback_rate_(0.0f) {
}

AudioRendererAlgorithmBase::~AudioRendererAlgorithmBase() {}

void AudioRendererAlgorithmBase::Initialize(int channels,
                                            int sample_bits,
                                            float initial_playback_rate,
                                            RequestReadCallback* callback) {
  DCHECK_GT(channels, 0);
  DCHECK_GT(sample_bits, 0);
  DCHECK(callback);
  DCHECK_EQ(sample_bits % 8, 0) << "We only support 8, 16, 32 bit audio.";

  channels_ = channels;
  sample_bytes_ = sample_bits / 8;
  request_read_callback_.reset(callback);

  set_playback_rate(initial_playback_rate);

  // Do the initial read.
  request_read_callback_->Run();
}

void AudioRendererAlgorithmBase::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  queue_.Clear();
  request_read_callback_->Run();
}

void AudioRendererAlgorithmBase::EnqueueBuffer(Buffer* buffer_in) {
  // If we're at end of stream, |buffer_in| contains no data.
  if (!buffer_in->IsEndOfStream())
    queue_.Enqueue(buffer_in);

  // If we still don't have enough data, request more.
  if (queue_.SizeInBytes() < kDefaultMaxQueueSizeInBytes)
    request_read_callback_->Run();
}

float AudioRendererAlgorithmBase::playback_rate() {
  return playback_rate_;
}

void AudioRendererAlgorithmBase::set_playback_rate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;
}

void AudioRendererAlgorithmBase::AdvanceInputPosition(size_t bytes) {
  queue_.Consume(bytes);

  if (queue_.SizeInBytes() < kDefaultMaxQueueSizeInBytes)
      request_read_callback_->Run();
}

size_t AudioRendererAlgorithmBase::CopyFromInput(uint8* dest, size_t bytes) {
  return queue_.Copy(dest, bytes);
}

bool AudioRendererAlgorithmBase::IsQueueEmpty() {
  return queue_.IsEmpty();
}

size_t AudioRendererAlgorithmBase::QueueSize() {
  return queue_.SizeInBytes();
}

int AudioRendererAlgorithmBase::channels() {
  return channels_;
}

int AudioRendererAlgorithmBase::sample_bytes() {
  return sample_bytes_;
}

}  // namespace media
