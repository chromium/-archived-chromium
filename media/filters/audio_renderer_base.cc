// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

// The maximum size of the queue, which also acts as the number of initial reads
// to perform for buffering.  The size of the queue should never exceed this
// number since we read only after we've dequeued and released a buffer in
// callback thread.
//
// This is sort of a magic number, but for 44.1kHz stereo audio this will give
// us enough data to fill approximately 4 complete callback buffers.
const size_t AudioRendererBase::kDefaultMaxQueueSize = 16;

AudioRendererBase::AudioRendererBase(size_t max_queue_size)
    : decoder_(NULL),
      max_queue_size_(max_queue_size),
      data_offset_(0),
      initialized_(false),
      stopped_(false) {
}

AudioRendererBase::~AudioRendererBase() {
  // Stop() should have been called and OnReadComplete() should have stopped
  // enqueuing data.
  DCHECK(stopped_);
  DCHECK(queue_.empty());
}

void AudioRendererBase::Stop() {
  OnStop();

  AutoLock auto_lock(lock_);
  while (!queue_.empty()) {
    queue_.front()->Release();
    queue_.pop_front();
  }
  stopped_ = true;
}

bool AudioRendererBase::Initialize(AudioDecoder* decoder) {
  DCHECK(decoder);
  decoder_ = decoder;

  // Schedule our initial reads.
  for (size_t i = 0; i < max_queue_size_; ++i) {
    ScheduleRead();
  }

  // Defer initialization until all scheduled reads have completed.
  return OnInitialize(decoder_->media_format());
}

void AudioRendererBase::OnReadComplete(Buffer* buffer_in) {
  bool initialization_complete = false;
  {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      buffer_in->AddRef();
      queue_.push_back(buffer_in);
      DCHECK(queue_.size() <= max_queue_size_);
    }

    // See if we're finally initialized.
    // TODO(scherkus): handle end of stream cases where we'll never reach max
    // queue size.
    if (!initialized_ && queue_.size() == max_queue_size_) {
      initialization_complete = true;
    }
  }

  if (initialization_complete) {
    initialized_ = true;
    host_->InitializationComplete();
  }
}

size_t AudioRendererBase::FillBuffer(uint8* dest, size_t len) {
  // Update the pipeline's time.
  host_->SetTime(last_fill_buffer_time_);

  size_t result = 0;
  size_t buffers_released = 0;
  {
    AutoLock auto_lock(lock_);
    // Loop until the buffer has been filled.
    while (len > 0 && !queue_.empty()) {
      Buffer* buffer = queue_.front();

      // Determine how much to copy.
      const uint8* data = buffer->GetData() + data_offset_;
      size_t data_len = buffer->GetDataSize() - data_offset_;
      data_len = std::min(len, data_len);

      // Copy into buffer.
      memcpy(dest, data, data_len);
      len -= data_len;
      dest += data_len;
      data_offset_ += data_len;
      result += data_len;

      // If we're done with the read, compute the time.
      if (len == 0) {
        // Integer divide so multiply before divide to work properly.
        int64 us_written = (buffer->GetDuration().InMicroseconds() *
                            data_offset_) / buffer->GetDataSize();
        last_fill_buffer_time_ = buffer->GetTimestamp() +
                                 base::TimeDelta::FromMicroseconds(us_written);
      }

      // Check to see if we're finished with the front buffer.
      if (data_offset_ == buffer->GetDataSize()) {
        // Update the time.  If this is the last buffer in the queue, we'll
        // drop out of the loop before len == 0, so we need to always update
        // the time here.
        last_fill_buffer_time_ = buffer->GetTimestamp() + buffer->GetDuration();

        // Dequeue the buffer.
        queue_.pop_front();
        buffer->Release();
        ++buffers_released;

        // Reset our offset into the front buffer.
        data_offset_ = 0;
      }
    }
  }

  // If we've released any buffers, read more buffers from the decoder.
  for (size_t i = 0; i < buffers_released; ++i) {
    ScheduleRead();
  }

  return result;
}

void AudioRendererBase::ScheduleRead() {
  host_->PostTask(NewRunnableMethod(decoder_, &AudioDecoder::Read,
      NewCallback(this, &AudioRendererBase::OnReadComplete)));
}

// static
bool AudioRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* channels_out,
                                         int* sample_rate_out,
                                         int* sample_bits_out) {
  // TODO(scherkus): might be handy to support NULL parameters.
  std::string mime_type;
  return media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      media_format.GetAsInteger(MediaFormat::kChannels, channels_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleRate, sample_rate_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleBits, sample_bits_out) &&
      mime_type.compare(mime_type::kUncompressedAudio) == 0;
}

}  // namespace media
