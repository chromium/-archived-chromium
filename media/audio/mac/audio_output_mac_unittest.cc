// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "media/audio/audio_output.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that can it be created and closed.
TEST(MacAudioTest, PCMWaveStreamGetAndClose) {
  // TODO(cpu) : skip if headless.
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST(MacAudioTest, PCMWaveStreamOpenAndClose) {
  // TODO(cpu) : skip if headless.
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(1024));
  oas->Close();
}
