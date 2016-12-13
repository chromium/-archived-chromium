// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "media/audio/linux/alsa_output.h"

namespace {
AudioManagerLinux* g_audio_manager = NULL;
}  // namespace

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioDevices() {
  // TODO(ajwong): Make this actually query audio devices.
  return true;
}

AudioOutputStream* AudioManagerLinux::MakeAudioStream(Format format,
                                                      int channels,
                                                      int sample_rate,
                                                      char bits_per_sample) {
  // TODO(ajwong): Do we want to be able to configure the device? default
  // should work correctly for all mono/stereo, but not surround, which needs
  // surround40, surround51, etc.
  //
  // http://0pointer.de/blog/projects/guide-to-sound-apis.html
  AlsaPCMOutputStream* stream =
      new AlsaPCMOutputStream(AlsaPCMOutputStream::kDefaultDevice,
                              100 /* 100ms minimal buffer */,
                              format, channels, sample_rate, bits_per_sample);
  return stream;
}

AudioManagerLinux::AudioManagerLinux() {
}

AudioManagerLinux::~AudioManagerLinux() {
}

void AudioManagerLinux::MuteAll() {
  // TODO(ajwong): Implement.
  NOTIMPLEMENTED();
}

void AudioManagerLinux::UnMuteAll() {
  // TODO(ajwong): Implement.
  NOTIMPLEMENTED();
}

const void* AudioManagerLinux::GetLastMockBuffer() {
  // TODO(ajwong): Implement.
  NOTIMPLEMENTED();
  return NULL;
}

// TODO(ajwong): Collapse this with the windows version.
void DestroyAudioManagerLinux(void* not_used) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerLinux();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerLinux, NULL);
  }
  return g_audio_manager;
}
