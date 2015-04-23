// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_thread.h"
#include "media/base/filter_host.h"

namespace {

// We will try to fill 200 ms worth of audio samples in each packet. A round
// trip latency for IPC messages are typically 10 ms, this should give us
// plenty of time to avoid clicks.
const int kMillisecondsPerPacket = 200;

// We have at most 3 packets in browser, i.e. 600 ms. This is a reasonable
// amount to avoid clicks.
const int kPacketsInBuffer = 3;

// We want to preroll 400 milliseconds before starting to play. Again, 400 ms
// of audio data should give us enough time to get more from the renderer.
const int kMillisecondsPreroll = 400;

}  // namespace

AudioRendererImpl::AudioRendererImpl(AudioMessageFilter* filter)
    : AudioRendererBase(kDefaultMaxQueueSize),
      channels_(0),
      sample_rate_(0),
      sample_bits_(0),
      bytes_per_second_(0),
      filter_(filter),
      stream_id_(0),
      shared_memory_(NULL),
      shared_memory_size_(0),
      io_loop_(filter->message_loop()),
      stopped_(false),
      playback_rate_(0.0f),
      pending_request_(false),
      prerolling_(true),
      preroll_bytes_(0) {
  DCHECK(io_loop_);
}

AudioRendererImpl::~AudioRendererImpl() {
}

base::TimeDelta AudioRendererImpl::ConvertToDuration(int bytes) {
  if (bytes_per_second_) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * bytes / bytes_per_second_);
  }
  return base::TimeDelta();
}

bool AudioRendererImpl::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int channels;
  int sample_rate;
  int sample_bits;
  return ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits);
}

bool AudioRendererImpl::OnInitialize(const media::MediaFormat& media_format) {
  // Parse integer values in MediaFormat.
  if (!ParseMediaFormat(media_format,
                        &channels_,
                        &sample_rate_,
                        &sample_bits_)) {
    return false;
  }

  // Create the audio output stream in browser process.
  bytes_per_second_ = sample_rate_ * channels_ * sample_bits_ / 8;
  size_t packet_size = bytes_per_second_ * kMillisecondsPerPacket / 1000;
  size_t buffer_capacity = packet_size * kPacketsInBuffer;

  // Calculate the amount for prerolling.
  preroll_bytes_ = bytes_per_second_ * kMillisecondsPreroll / 1000;

  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnCreateStream,
          AudioManager::AUDIO_PCM_LINEAR, channels_, sample_rate_, sample_bits_,
          packet_size, buffer_capacity));
  return true;
}

void AudioRendererImpl::OnStop() {
  AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  stopped_ = true;

  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnDestroy));
}

void AudioRendererImpl::OnReadComplete(media::Buffer* buffer_in) {
  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  // TODO(hclam): handle end of stream here.

  // Use the base class to queue the buffer.
  AudioRendererBase::OnReadComplete(buffer_in);

  // Post a task to render thread to notify a packet reception.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnNotifyPacketReady));
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  DCHECK(rate >= 0.0f);

  // We have two cases here:
  // Play: playback_rate_ == 0.0 && rate != 0.0
  // Pause: playback_rate_ != 0.0 && rate == 0.0
  AutoLock auto_lock(lock_);
  if (playback_rate_ == 0.0f && rate != 0.0f) {
    // Play is a bit tricky, we can only play if we have done prerolling.
    // TODO(hclam): I should check for end of streams status here.
    if (!prerolling_)
      io_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &AudioRendererImpl::OnPlay));
  } else if (playback_rate_ != 0.0f && rate == 0.0f) {
    // Pause is easy, we can always pause.
    io_loop_->PostTask(FROM_HERE,
                       NewRunnableMethod(this, &AudioRendererImpl::OnPause));
  }
  playback_rate_ = rate;

  // If we are playing, give a kick to try fulfilling the packet request as
  // the previous packet request may be stalled by a pause.
  if (rate > 0.0f) {
    io_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::OnNotifyPacketReady));
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  // TODO(hclam): change this to multichannel if possible.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(
          this, &AudioRendererImpl::OnSetVolume, volume, volume));
}

void AudioRendererImpl::OnCreated(base::SharedMemoryHandle handle,
                                  size_t length) {
  DCHECK(MessageLoop::current() == io_loop_);

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;
}

void AudioRendererImpl::OnRequestPacket(size_t bytes_in_buffer,
                                        const base::Time& message_timestamp) {
  DCHECK(MessageLoop::current() == io_loop_);

  {
    AutoLock auto_lock(lock_);
    DCHECK(!pending_request_);
    pending_request_ = true;

    // Use the information provided by the IPC message to adjust the playback
    // delay.
    request_timestamp_ = message_timestamp;
    request_delay_ = ConvertToDuration(bytes_in_buffer);
  }

  // Try to fill in the fulfil the packet request.
  OnNotifyPacketReady();
}

void AudioRendererImpl::OnStateChanged(AudioOutputStream::State state,
                                       int info) {
  DCHECK(MessageLoop::current() == io_loop_);

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  switch (state) {
    case AudioOutputStream::STATE_ERROR:
      host()->Error(media::PIPELINE_ERROR_AUDIO_HARDWARE);
      break;
    // TODO(hclam): handle these events.
    case AudioOutputStream::STATE_STARTED:
    case AudioOutputStream::STATE_PAUSED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioRendererImpl::OnVolume(double left, double right) {
  // TODO(hclam): decide whether we need to report the current volume to
  // pipeline.
}

void AudioRendererImpl::OnCreateStream(
    AudioManager::Format format, int channels, int sample_rate,
    int bits_per_sample, size_t packet_size, size_t buffer_capacity) {
  DCHECK(MessageLoop::current() == io_loop_);

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  // Make sure we don't call create more than once.
  DCHECK_EQ(0, stream_id_);
  stream_id_ = filter_->AddDelegate(this);

  ViewHostMsg_Audio_CreateStream params;
  params.format = format;
  params.channels = channels;
  params.sample_rate = sample_rate;
  params.bits_per_sample = bits_per_sample;
  params.packet_size = packet_size;
  params.buffer_capacity = buffer_capacity;

  filter_->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params));
}

void AudioRendererImpl::OnPlay() {
  DCHECK(MessageLoop::current() == io_loop_);

  filter_->Send(new ViewHostMsg_StartAudioStream(0, stream_id_));
}

void AudioRendererImpl::OnPause() {
  DCHECK(MessageLoop::current() == io_loop_);

  filter_->Send(new ViewHostMsg_PauseAudioStream(0, stream_id_));
}

void AudioRendererImpl::OnDestroy() {
  DCHECK(MessageLoop::current() == io_loop_);

  filter_->RemoveDelegate(stream_id_);
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
}

void AudioRendererImpl::OnSetVolume(double left, double right) {
  DCHECK(MessageLoop::current() == io_loop_);

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  filter_->Send(new ViewHostMsg_SetAudioVolume(0, stream_id_, left, right));
}

void AudioRendererImpl::OnNotifyPacketReady() {
  DCHECK(MessageLoop::current() == io_loop_);

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  if (pending_request_ && playback_rate_ > 0.0f) {
    DCHECK(shared_memory_.get());

    // Adjust the playback delay.
    base::Time current_time = base::Time::Now();

    // Save a local copy of the request delay.
    base::TimeDelta request_delay = request_delay_;
    if (current_time > request_timestamp_) {
      base::TimeDelta receive_latency = current_time - request_timestamp_;

      // If the receive latency is too much it may offset all the delay.
      if (receive_latency >= request_delay) {
        request_delay = base::TimeDelta();
      } else {
        request_delay -= receive_latency;
      }
    }

    size_t filled = FillBuffer(static_cast<uint8*>(shared_memory_->memory()),
                               shared_memory_size_,
                               playback_rate_,
                               request_delay);
    // TODO(hclam): we should try to fill in the buffer as much as possible.
    if (filled > 0) {
      pending_request_ = false;
      request_delay_ = base::TimeDelta();
      request_timestamp_ = base::Time();
      // Then tell browser process we are done filling into the buffer.
      filter_->Send(
          new ViewHostMsg_NotifyAudioPacketReady(0, stream_id_, filled));

      if (prerolling_) {
        // We have completed prerolling.
        if (filled > preroll_bytes_) {
          prerolling_ = false;
          preroll_bytes_ = 0;
          filter_->Send(new ViewHostMsg_StartAudioStream(0, stream_id_));
        } else {
          preroll_bytes_ -= filled;
        }
      }
    }
  }
}
