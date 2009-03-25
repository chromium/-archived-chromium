// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "media/audio/mac/audio_output_mac.h"

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerMac : public AudioManager {
 public:
  AudioManagerMac() {
  }
  
  virtual bool HasAudioDevices() {
    AudioDeviceID output_device_id = 0;
    size_t size = sizeof(output_device_id);
    OSStatus err = AudioHardwareGetProperty(
        kAudioHardwarePropertyDefaultOutputDevice, &size, &output_device_id);
    return ((err == noErr) && (output_device_id > 0));
  }

  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample) {
    // TODO(cpu): add mock format.
    if (format != AUDIO_PCM_LINEAR)
      return NULL;
    return new PCMQueueOutAudioOutputStream(this, channels, sample_rate,
                                            bits_per_sample);
  }

  virtual void MuteAll() {
    // TODO(cpu): implement.
  }

  virtual void UnMuteAll() {
    // TODO(cpu): implement.
  }

  virtual const void* GetLastMockBuffer() {
    // TODO(cpu): implement.
    return NULL;
  }

  // Called by the stream when it has been released by calling Close().
  void ReleaseStream(PCMQueueOutAudioOutputStream* stream) {
    delete stream;
  }

 private:
  virtual ~AudioManagerMac() {}
  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

// By convention, the AudioManager is not thread safe.
AudioManager* AudioManager::GetAudioManager() {
  // TODO(cpu): Do not leak singleton.
  static AudioManagerMac* instance = NULL;
  if (!instance) {
    instance = new AudioManagerMac();
  }
  return instance;
}

