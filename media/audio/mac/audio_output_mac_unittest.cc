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

// This test produces actual audio for 1.5 seconds on the default wave device at
// 44.1K s/sec. Parameters have been chosen carefully so you should not hear
// pops or noises while the sound is playing. The sound must also be identical
// to the sound of PCMWaveStreamPlay200HzTone22KssMono test.
TEST(MacAudioTest, PCMWaveStreamPlay200HzTone44KssMono) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
  audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                             AudioManager::kAudioCDSampleRate, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 10) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));
  oas->Start(&source);
  usleep(1500000);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave device at
// 22K s/sec. Parameters have been chosen carefully so you should not hear pops
// or noises while the sound is playing. The sound must also be identical to the
// sound of PCMWaveStreamPlay200HzTone44KssMono test.
TEST(MacAudioTest, PCMWaveStreamPlay200HzTone22KssMono) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
  audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                             AudioManager::kAudioCDSampleRate/2, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate/2);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 20) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));
  oas->Start(&source);
  usleep(1500000);
  oas->Stop();
  oas->Close();
}
