// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include "base/basictypes.h"
#include "media/audio/audio_output.h"

class PCMQueueOutAudioOutputStream;

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerMac : public AudioManager {
 public:
  AudioManagerMac() {};

  // Implementation of AudioManager.
  virtual bool HasAudioDevices();
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample);
  virtual void MuteAll();
  virtual void UnMuteAll();
  virtual const void* GetLastMockBuffer();

  // Mac-only method to free a stream created in MakeAudioStream.
  // It is called internally by the audio stream when it has been closed.
  void ReleaseStream(PCMQueueOutAudioOutputStream* stream);

 private:
  friend void DestroyAudioManagerMac(void*);
  virtual ~AudioManagerMac() {};
  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
