// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "media/audio/audio_output.h"

class PCMWaveOutAudioOutputStream;
class AudioOutputStreamMockWin;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerWin : public AudioManager {
 public:
  AudioManagerWin() {}
  // Implementation of AudioManager.
  virtual bool HasAudioDevices();
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample);
  virtual void MuteAll();
  virtual void UnMuteAll();
  virtual const void* GetLastMockBuffer();

  // Windows-only methods to free a stream created in MakeAudioStream. These
  // are called internally by the audio stream when it has been closed.
  void ReleaseStream(PCMWaveOutAudioOutputStream* stream);
  void ReleaseStream(AudioOutputStreamMockWin* stream);

 private:
  friend void DestroyAudioManagerWin(void *);
  virtual ~AudioManagerWin();
  DISALLOW_COPY_AND_ASSIGN(AudioManagerWin);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_

