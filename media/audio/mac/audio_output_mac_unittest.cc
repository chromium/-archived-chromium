// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"


// Validate that the SineWaveAudioSource writes the expected values for
// the FORMAT_16BIT_MONO. 
TEST(MacAudioTest, SineWaveAudio16MonoTest) {
  const size_t samples = 1024;
  const int freq = 200;
  
  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             freq, AudioManager::kTelephoneSampleRate);
  
  // TODO(cpu): Put the real test when the mock renderer is ported.
  int16 buffer[samples] = { 0xffff };
  source.OnMoreData(NULL, buffer, sizeof(buffer));
  EXPECT_EQ(0, buffer[0]);
  EXPECT_EQ(5126, buffer[1]);
}

// ===========================================================================
// Validation of AudioManager::AUDIO_PCM_LINEAR
//
// Unlike windows, the tests can reliably detect the existense of real
// audio devices on the bots thus no need for 'headless' detection.

// Test that can it be created and closed.
TEST(MacAudioTest, PCMWaveStreamGetAndClose) {
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
