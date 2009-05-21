// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_

#include "base/thread.h"
#include "media/audio/audio_output.h"

class AudioManagerLinux : public AudioManager {
 public:
  AudioManagerLinux();

  // Implementation of AudioManager.
  virtual bool HasAudioDevices();
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample);
  virtual void MuteAll();
  virtual void UnMuteAll();
  virtual const void* GetLastMockBuffer();

 private:
  // Friend function for invoking the private destructor at exit.
  friend void DestroyAudioManagerLinux(void*);
  virtual ~AudioManagerLinux();

  DISALLOW_COPY_AND_ASSIGN(AudioManagerLinux);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
