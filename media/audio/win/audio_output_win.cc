// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output.h"

#include <windows.h>
#include <mmsystem.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/waveout_output_win.h"

// A do-nothing audio stream. It behaves like a regular audio stream but does
// not have any side effect, except possibly the creation and tear-down of
// of a thread. It is useful to test code that uses audio streams such as
// audio sources.
class AudioOutputStreamMockWin : public AudioOutputStream {
 public:
  explicit AudioOutputStreamMockWin(AudioManagerWin* manager)
      : manager_(manager),
        callback_(NULL),
        buffer_(NULL),
        packet_size_(0),
        left_volume_(1.0),
        right_volume_(1.0) {
  }

  virtual ~AudioOutputStreamMockWin() {
    delete[] buffer_;
    packet_size_ = 0;
  }

  virtual bool Open(size_t packet_size) {
    if (packet_size < sizeof(int16))
      return false;
    packet_size_ = packet_size;
    buffer_ = new char[packet_size_];
    return true;
  }

  virtual void Start(AudioSourceCallback* callback)  {
    callback_ = callback;
    memset(buffer_, 0, packet_size_);
    callback_->OnMoreData(this, buffer_, packet_size_);
  }

  // TODO(cpu): flesh out Start and Stop methods. We need a thread to
  // perform periodic callbacks.
  virtual void Stop() {
  }

  virtual void SetVolume(double left_level, double right_level) {
    left_volume_ = left_level;
    right_volume_ = right_level;
  }

  virtual void GetVolume(double* left_level, double* right_level) {
    *left_level = left_volume_;
    *right_level = right_volume_;
  }

  virtual size_t GetNumBuffers() {
    return 1;
  }

  virtual void Close() {
    callback_->OnClose(this);
    callback_ = NULL;
    manager_->ReleaseStream(this);
  }

  char* buffer() {
    return buffer_;
  }

 private:
  AudioManagerWin* manager_;
  AudioSourceCallback* callback_;
  char* buffer_;
  size_t packet_size_;
  double left_volume_;
  double right_volume_;
};

namespace {

// The next 3 constants are some sensible limits to prevent integer overflow
// at this layer.
// TODO(cpu): Some day expand the support and API to more than stereo.
const int kMaxChannels = 2;
const int kMaxSampleRate = 192000;
const int kMaxBitsPerSample = 64;

AudioOutputStreamMockWin* g_last_mock_stream = NULL;
AudioManagerWin* g_audio_manager = NULL;

void ReplaceLastMockStream(AudioOutputStreamMockWin* newer) {
  if (g_last_mock_stream)
    delete g_last_mock_stream;
  g_last_mock_stream = newer;
}

}  // namespace.


bool AudioManagerWin::HasAudioDevices() {
  return (::waveOutGetNumDevs() != 0);
}

// Factory for the implementations of AudioOutputStream. Two implementations
// should suffice most windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOutWrite API (in progress)
// - PCMDXSoundAudioOutputStream: Based on DirectSound or XAudio (future work).

AudioOutputStream* AudioManagerWin::MakeAudioStream(Format format, int channels,
                                                    int sample_rate,
                                                    char bits_per_sample) {
  if ((channels > kMaxChannels) || (channels <= 0) ||
      (sample_rate > kMaxSampleRate) || (sample_rate <= 0) ||
      (bits_per_sample > kMaxBitsPerSample) || (bits_per_sample <= 0))
    return NULL;

  if (format == AUDIO_MOCK) {
    return new AudioOutputStreamMockWin(this);
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMWaveOutAudioOutputStream(this, channels, sample_rate,
                                           bits_per_sample, WAVE_MAPPER);
  }
  return NULL;
}

void AudioManagerWin::ReleaseStream(PCMWaveOutAudioOutputStream* stream) {
  if (stream)
    delete stream;
}

void AudioManagerWin::ReleaseStream(AudioOutputStreamMockWin *stream) {
  // Note that we keep the last mock stream so GetLastMockBuffer() works.
  ReplaceLastMockStream(stream);
}

const void* AudioManagerWin::GetLastMockBuffer() {
  return (g_last_mock_stream) ? g_last_mock_stream->buffer() : NULL;
}

void AudioManagerWin::MuteAll() {
}

void AudioManagerWin::UnMuteAll() {
}

AudioManagerWin::~AudioManagerWin() {
  ReplaceLastMockStream(NULL);
}

void DestroyAudioManagerWin(void* param) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerWin();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerWin, NULL);
  }
  return g_audio_manager;
}
