// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_thread.h"
#include "media/base/filter_host.h"

// We'll try to fill 8192 samples per buffer, which is roughly ~194ms of audio
// data for a 44.1kHz audio source.
static const size_t kSamplesPerBuffer = 8192;

AudioRendererImpl::AudioRendererImpl(AudioMessageFilter* filter)
    : AudioRendererBase(kDefaultMaxQueueSize),
      filter_(filter),
      stream_id_(0),
      shared_memory_(NULL),
      shared_memory_size_(0),
      io_loop_(filter->message_loop()),
      stopped_(false),
      playback_rate_(0.0f),
      packet_request_event_(true, false) {
  DCHECK(io_loop_);
}

AudioRendererImpl::~AudioRendererImpl() {
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
  int channels;
  int sample_rate;
  int sample_bits;
  if (!ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits)) {
    return false;
  }

  // Create the audio output stream in browser process.
  size_t packet_size = kSamplesPerBuffer * channels * sample_bits / 8;
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnCreateStream,
          AudioManager::AUDIO_PCM_LINEAR, channels, sample_rate, sample_bits,
          packet_size));
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
  // Use the base class to queue the buffer.
  AudioRendererBase::OnReadComplete(buffer_in);
  // Post a task to render thread to notify a packet reception.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnNotifyPacketReady));
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  // TODO(hclam): This is silly.  We should use a playback rate of != 1.0 to
  // stop the audio stream.  This does not work right now, so we just check
  // for this in OnNotifyPacketReady().
  playback_rate_ = rate;
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

  filter_->Send(new ViewHostMsg_StartAudioStream(0, stream_id_));
}

void AudioRendererImpl::OnRequestPacket() {
  DCHECK(MessageLoop::current() == io_loop_);

  packet_request_event_.Signal();

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
      host_->Error(media::PIPELINE_ERROR_AUDIO_HARDWARE);
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
    int bits_per_sample, size_t packet_size) {
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

  filter_->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params));
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
  if (packet_request_event_.IsSignaled()) {
    size_t filled = 0;
    DCHECK(shared_memory_.get());
    // TODO(hclam):  This is a hack.  The stream should be stopped.
    if (playback_rate_ > 0.0f) {
      filled = FillBuffer(static_cast<uint8*>(shared_memory_->memory()),
                          shared_memory_size_, playback_rate_);
    } else {
      memset(shared_memory_->memory(), 0, shared_memory_size_);
      filled = shared_memory_size_;
    }
    if (filled > 0) {
      packet_request_event_.Reset();
      // Then tell browser process we are done filling into the buffer.
      filter_->Send(
          new ViewHostMsg_NotifyAudioPacketReady(0, stream_id_, filled));
    }
  }
}
