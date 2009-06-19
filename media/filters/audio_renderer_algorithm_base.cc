// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

namespace media {

// The maximum size of the queue, which also acts as the number of initial reads
// to perform for buffering.  The size of the queue should never exceed this
// number since we read only after we've dequeued and released a buffer in
// callback thread.
//
// This is sort of a magic number, but for 44.1kHz stereo audio this will give
// us enough data to fill approximately 4 complete callback buffers.
const size_t kDefaultMaxQueueSize = 16;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      sample_rate_(0),
      sample_bits_(0),
      playback_rate_(0.0f),
      expected_queue_size_(0) {
}

AudioRendererAlgorithmBase::~AudioRendererAlgorithmBase() {}

void AudioRendererAlgorithmBase::Initialize(int channels,
                                            int sample_rate,
                                            int sample_bits,
                                            float initial_playback_rate,
                                            RequestReadCallback* callback) {
  DCHECK_GT(channels, 0);
  DCHECK_GT(sample_rate, 0);
  DCHECK_GT(sample_bits, 0);
  DCHECK_GE(initial_playback_rate, 0.0);
  DCHECK(callback);

  channels_ = channels;
  sample_rate_ = sample_rate;
  sample_bits_ = sample_bits;
  playback_rate_ = initial_playback_rate;
  request_read_callback_.reset(callback);
  FillQueue();
}

void AudioRendererAlgorithmBase::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  queue_.clear();
}

void AudioRendererAlgorithmBase::EnqueueBuffer(Buffer* buffer_in) {
  // If we're at end of stream, |buffer_in| contains no data.
  if (!buffer_in->IsEndOfStream()) {
    queue_.push_back(buffer_in);
    DCHECK_LE(queue_.size(), kDefaultMaxQueueSize);
  }
}

void AudioRendererAlgorithmBase::SetPlaybackRate(float new_rate) {
  playback_rate_ = new_rate;
}

void AudioRendererAlgorithmBase::FillQueue() {
  for ( ; expected_queue_size_ < kDefaultMaxQueueSize; ++expected_queue_size_)
    request_read_callback_->Run();
}

}  // namespace media
