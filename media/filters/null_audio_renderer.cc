// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host.h"
#include "media/filters/null_audio_renderer.h"

namespace media {

// The number of reads to perform during initialization for preroll purposes.
static const size_t kInitialReads = 16;

// The number of buffers we consume before sleeping.  By doing so we can sleep
// for longer, reducing CPU load and avoiding situations where audio samples
// are so short that the OS sleeps our thread for too long.
static const size_t kBuffersPerSleep = 4;

NullAudioRenderer::NullAudioRenderer()
    : decoder_(NULL),
      playback_rate_(0.0f),
      thread_(NULL),
      initialized_(false),
      shutdown_(false) {
}

NullAudioRenderer::~NullAudioRenderer() {
  Stop();
}

// static
bool NullAudioRenderer::IsMediaFormatSupported(
    const MediaFormat* media_format) {
  DCHECK(media_format);
  std::string mime_type;
  return media_format->GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type.compare(mime_type::kUncompressedAudio) == 0;
}

void NullAudioRenderer::Stop() {
  shutdown_ = true;
  if (thread_)
    PlatformThread::Join(thread_);
}

void NullAudioRenderer::SetPlaybackRate(float playback_rate) {
  playback_rate_ = playback_rate;
}

bool NullAudioRenderer::Initialize(AudioDecoder* decoder) {
  DCHECK(decoder);
  decoder_ = decoder;

  // It's safe to start the thread now because it simply sleeps when playback
  // rate is 0.0f.
  if (!PlatformThread::Create(0, this, &thread_))
    return false;

  // Schedule our initial reads.
  for (size_t i = 0; i < kInitialReads; ++i) {
    ScheduleRead();
  }

  // Defer initialization until all scheduled reads have completed.
  return true;
}

void NullAudioRenderer::SetVolume(float volume) {
  // Do nothing.
}

void NullAudioRenderer::OnAssignment(Buffer* buffer_in) {
  bool initialized = false;
  {
    AutoLock auto_lock(input_lock_);
    buffer_in->AddRef();
    input_queue_.push_back(buffer_in);
    DCHECK(input_queue_.size() <= kInitialReads);

    // See if we're finally initialized.
    // TODO(scherkus): handle end of stream.
    initialized = !initialized_ && input_queue_.size() == kInitialReads;
  }

  if (initialized) {
    initialized_ = true;
    host_->InitializationComplete();
  }
}

void NullAudioRenderer::ThreadMain() {
  // Loop until we're signaled to stop.
  while (!shutdown_) {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
    int sleep_ms = 0;
    int released_buffers = 0;

    // Only consume buffers when actually playing.
    if (playback_rate_ > 0.0f) {
      AutoLock auto_lock(input_lock_);
      for (size_t i = 0; i < kBuffersPerSleep && !input_queue_.empty(); ++i) {
        scoped_refptr<Buffer> buffer = input_queue_.front();
        input_queue_.pop_front();
        buffer->Release();
        timestamp = buffer->GetTimestamp();
        duration += buffer->GetDuration();
        ++released_buffers;
      }
      // Apply the playback rate to our sleep duration.
      sleep_ms =
          static_cast<int>(floor(duration.InMillisecondsF() / playback_rate_));
    }

    // Schedule reads for every released buffer to maintain "playback".
    for (int i = 0; i < released_buffers; ++i) {
      ScheduleRead();
    }

    // Sleep and update the clock when we wake up.
    PlatformThread::Sleep(sleep_ms);
    if (timestamp.InMicroseconds() > 0) {
      host_->SetTime(timestamp);
    }
  }
}

void NullAudioRenderer::ScheduleRead() {
  host_->PostTask(NewRunnableMethod(decoder_, &AudioDecoder::Read,
      new AssignableBuffer<NullAudioRenderer, Buffer>(this)));
}

}  // namespace media
