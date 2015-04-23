// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_impl.h"

namespace media {

// We'll try to fill 4096 samples per buffer, which is roughly ~92ms of audio
// data for a 44.1kHz audio source.
static const size_t kSamplesPerBuffer = 8*1024;

AudioRendererImpl::AudioRendererImpl()
    : AudioRendererBase(kDefaultMaxQueueSize),
      playback_rate_(0.0f),
      stream_(NULL) {
}

AudioRendererImpl::~AudioRendererImpl() {
  // Close down the audio device.
  if (stream_) {
    stream_->Stop();
    stream_->Close();
  }
}

bool AudioRendererImpl::IsMediaFormatSupported(
    const MediaFormat& media_format) {
  int channels;
  int sample_rate;
  int sample_bits;
  return AudioManager::GetAudioManager()->HasAudioDevices() &&
      ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits);
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  DCHECK(stream_);

  // TODO(fbarchard): limit rate to reasonable values
  playback_rate_ = rate;

  static bool started = false;
  if (rate > 0.0f && !started)
    stream_->Start(this);
}

void AudioRendererImpl::SetVolume(float volume) {
  stream_->SetVolume(volume, volume);
}

size_t AudioRendererImpl::OnMoreData(AudioOutputStream* stream, void* dest_void,
                                     size_t len) {
  // TODO(scherkus): handle end of stream.
  DCHECK(stream_ == stream);

  // TODO(scherkus): Maybe change OnMoreData to pass in char/uint8 or similar.
  // TODO(fbarchard): Waveout_output_win.h should handle zero length buffers
  //                  without clicking.
  return FillBuffer(static_cast<uint8*>(dest_void), len,
                    playback_rate_, base::TimeDelta());
}

void AudioRendererImpl::OnClose(AudioOutputStream* stream) {
  // TODO(scherkus): implement OnClose.
  NOTIMPLEMENTED();
}

void AudioRendererImpl::OnError(AudioOutputStream* stream, int code) {
  // TODO(scherkus): implement OnError.
  NOTIMPLEMENTED();
}

bool AudioRendererImpl::OnInitialize(const MediaFormat& media_format) {
  // Parse out audio parameters.
  int channels;
  int sample_rate;
  int sample_bits;
  if (!ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits)) {
    return false;
  }

  // Create our audio stream.
  stream_ = AudioManager::GetAudioManager()->MakeAudioStream(
      AudioManager::AUDIO_PCM_LINEAR, channels, sample_rate, sample_bits);
  DCHECK(stream_);
  if (!stream_)
    return false;

  // Calculate buffer size and open the stream.
  size_t size = kSamplesPerBuffer * channels * sample_bits / 8;
  if (!stream_->Open(size)) {
    stream_->Close();
    stream_ = NULL;
    return false;
  }
  return true;
}

void AudioRendererImpl::OnStop() {
  DCHECK(stream_);
  stream_->Stop();
}

}  // namespace media
