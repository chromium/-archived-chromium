// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "media/audio/audio_output.h"

// A do-nothing audio stream. It behaves like a regular audio stream but does
// not have any side effect, except possibly the creation and tear-down of
// of a thread. It is useful to test code that uses audio streams such as
// audio sources.
class AudioOutputStreamWinMock : public AudioOutputStream {
 public:
  AudioOutputStreamWinMock()
      : callback_(NULL),
        buffer_(NULL),
        packet_size_(0),
        left_volume_(0.0),
        right_volume_(0.0) {
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

  virtual void Close() {
    callback_->OnClose(this);
    callback_ = NULL;
    delete this;
  }

 protected:
  virtual ~AudioOutputStreamWinMock() {
    delete[] buffer_;
    packet_size_ = 0;
  }

 private:
  AudioSourceCallback* callback_;
  char* buffer_;
  size_t packet_size_;
  double left_volume_;
  double right_volume_;
};

class AudioManagerWin : public AudioManager {
 public:
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample) {
    if (format == AUDIO_MOCK)
      return new AudioOutputStreamWinMock();
    return NULL;
  }

  virtual void MuteAll() {
  }

  virtual void UnMuteAll() {
  }

 protected:
  virtual ~AudioManagerWin() {
  }
};

// TODO(cpu): Decide how to manage the lifetime of the AudioManager singleton.
// Right now we are leaking it.
AudioManager* GetAudioManager() {
  static AudioManagerWin* audio_manager = NULL;
  if (!audio_manager)
    audio_manager = new AudioManagerWin();
  return audio_manager;
}



