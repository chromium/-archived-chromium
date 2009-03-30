// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "media/base/filter_host.h"

// We'll try to fill 4096 samples per buffer, which is roughly ~92ms of audio
// data for a 44.1kHz audio source.
static const size_t kSamplesPerBuffer = 4096;

AudioRendererImpl::AudioRendererImpl(WebMediaPlayerDelegateImpl* delegate)
    : AudioRendererBase(kDefaultMaxQueueSize),
      delegate_(delegate),
      stream_id_(0),
      shared_memory_(NULL),
      shared_memory_size_(0),
      packet_requested_(false),
      render_loop_(RenderThread::current()->message_loop()),
      resource_release_event_(true, false) {
  // TODO(hclam): do we need to move this method call to render thread?
  delegate_->SetAudioRenderer(this);
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
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnCreateAudioStream,
          AudioManager::AUDIO_PCM_LINEAR, channels, sample_rate, sample_bits,
          packet_size));
  return true;
}

void AudioRendererImpl::OnStop() {
  if (!resource_release_event_.IsSignaled()) {
    render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &AudioRendererImpl::ReleaseResources, false));
    resource_release_event_.Wait();
  }
}

void AudioRendererImpl::OnReadComplete(media::Buffer* buffer_in) {
  // Use the base class to queue the buffer.
  AudioRendererBase::OnReadComplete(buffer_in);
  // Post a task to render thread to notify a packet reception.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnNotifyAudioPacketReady));
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  // TODO(hclam): handle playback rates not equal to 1.0.
  if (rate == 1.0f) {
    // TODO(hclam): what should I do here? OnCreated has fired StartAudioStream
    // in the browser process, it seems there's nothing to do here.
  } else {
    NOTIMPLEMENTED();
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  // TODO(hclam): change this to multichannel if possible.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(
          this, &AudioRendererImpl::OnSetAudioVolume, volume, volume));
}

void AudioRendererImpl::OnCreated(base::SharedMemoryHandle handle,
                                  size_t length) {
  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;
  // TODO(hclam): is there any better place to do this?
  OnStartAudioStream();
}

void AudioRendererImpl::OnRequestPacket() {
  packet_requested_ = true;
  // Post a task to render thread and try to grab a packet for sending back.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::OnNotifyAudioPacketReady));
}

void AudioRendererImpl::OnStateChanged(AudioOutputStream::State state,
                                       int info) {
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

void AudioRendererImpl::ReleaseResources(bool is_render_thread_dying) {
  if (!is_render_thread_dying)
    OnCloseAudioStream();
  resource_release_event_.Signal();
}

void AudioRendererImpl::OnCreateAudioStream(
    AudioManager::Format format, int channels, int sample_rate,
    int bits_per_sample, size_t packet_size) {
  stream_id_ = delegate_->view()->CreateAudioStream(
      this, format, channels, sample_rate, bits_per_sample, packet_size);
}

void AudioRendererImpl::OnStartAudioStream() {
  delegate_->view()->StartAudioStream(stream_id_);
}

void AudioRendererImpl::OnCloseAudioStream() {
  // Unregister ourself from RenderView, we will not be called anymore.
  delegate_->view()->CloseAudioStream(stream_id_);
}

void AudioRendererImpl::OnSetAudioVolume(double left, double right) {
  delegate_->view()->SetAudioVolume(stream_id_, left, right);
}

void AudioRendererImpl::OnNotifyAudioPacketReady() {
  if (packet_requested_) {
    DCHECK(shared_memory_.get());
    // Fill into the shared memory.
    size_t filled = FillBuffer(static_cast<uint8*>(shared_memory_->memory()),
                               shared_memory_size_);
    if (filled > 0) {
      packet_requested_ = false;
      // Then tell browser process we are done filling into the buffer.
      delegate_->view()->NotifyAudioPacketReady(stream_id_, filled);
    }
  }
}
