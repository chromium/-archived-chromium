// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestSourceBasic : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit TestSourceBasic()
      : callback_count_(0),
        had_error_(0),
        was_closed_(0) {
  }

  // AudioSourceCallback::OnMoreData implementation:
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size) {
    ++callback_count_;
    // Touch the first byte to make sure memory is good.
    if (max_size)
      reinterpret_cast<char*>(dest)[0] = 1;
    return max_size;
  }

  // AudioSourceCallback::OnClose implementation:
  virtual void OnClose(AudioOutputStream* stream) {
    ++was_closed_;
  }

  // AudioSourceCallback::OnError implementation:
  virtual void OnError(AudioOutputStream* stream, int code) {
    ++had_error_;
  }

  // Returns how many times OnMoreData() has been called.
  int callback_count() const {
    return callback_count_;
  }

  // Returns how many times the OnError callback was called.
  int had_error() const {
    return had_error_;
  }

  void set_error(bool error) {
    had_error_ += error ? 1 : 0;
  }

  // Returns how many times the OnClose callback was called.
  int was_closed() const {
    return was_closed_;
  }

 private:
  int callback_count_;
  int had_error_;
  int was_closed_;
};

// Specializes TestSourceBasic to detect that the AudioStream is using
// double buffering correctly.
class TestSourceDoubleBuffer : public TestSourceBasic {
 public:
  TestSourceDoubleBuffer() {
    buffer_address_[0] = NULL;
    buffer_address_[1] = NULL;
  }
  // Override of TestSourceBasic::OnMoreData.
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size) {
    // Call the base, which increments the callback_count_.
    TestSourceBasic::OnMoreData(stream, dest, max_size);
    if (callback_count() % 2) {
      set_error(!CompareExistingIfNotNULL(1, dest));
    } else {
      set_error(!CompareExistingIfNotNULL(0, dest));
    }
    if (callback_count() > 2) {
      set_error(buffer_address_[0] == buffer_address_[1]);
    }
    return max_size;
  }

 private:
  bool CompareExistingIfNotNULL(size_t index, void* address) {
    void*& entry = buffer_address_[index];
    if (!entry)
      entry = address;
    return (entry == address);
  }

  void* buffer_address_[2];
};



// ============================================================================
// Validate that the AudioManager::AUDIO_MOCK callbacks work.
TEST(WinAudioTest, MockStreamBasicCallbacks) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_MOCK, 2, 8000, 8);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(256));
  TestSourceBasic source;
  oas->Start(&source);
  EXPECT_GT(source.callback_count(), 0);
  oas->Stop();
  oas->Close();
  EXPECT_EQ(0, source.had_error());
  EXPECT_EQ(1, source.was_closed());
}

// Validate that the SineWaveAudioSource writes the expected values for
// the FORMAT_16BIT_MONO. The values are carefully selected so rounding issues
// do not affect the result. We also test that AudioManager::GetLastMockBuffer
// works.
TEST(WinAudioTest, SineWaveAudio16MonoTest) {
  const size_t samples = 1024;
  const size_t bytes_per_sample = 2;
  const int freq = 200;

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             freq, AudioManager::kTelephoneSampleRate);

  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_MOCK, 1,
                                 AudioManager::kTelephoneSampleRate,
                                 bytes_per_sample * 2);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(samples * bytes_per_sample));

  oas->Start(&source);
  oas->Stop();
  oas->Close();

  const int16* last_buffer =
      reinterpret_cast<const int16*>(audio_man->GetLastMockBuffer());
  ASSERT_TRUE(NULL != last_buffer);

  size_t half_period = AudioManager::kTelephoneSampleRate / (freq * 2);

  // Spot test positive incursion of sine wave.
  EXPECT_EQ(0, last_buffer[0]);
  EXPECT_EQ(5126, last_buffer[1]);
  EXPECT_TRUE(last_buffer[1] < last_buffer[2]);
  EXPECT_TRUE(last_buffer[2] < last_buffer[3]);
  // Spot test negative incursion of sine wave.
  EXPECT_EQ(0, last_buffer[half_period]);
  EXPECT_EQ(-5126, last_buffer[half_period + 1]);
  EXPECT_TRUE(last_buffer[half_period + 1] > last_buffer[half_period + 2]);
  EXPECT_TRUE(last_buffer[half_period + 2] > last_buffer[half_period + 3]);
}

// ===========================================================================
// Validation of AudioManager::AUDIO_PCM_LINEAR

// Test that can it be created and closed.
TEST(WinAudioTest, PCMWaveStreamGetAndClose) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST(WinAudioTest, DISABLED_PCMWaveStreamOpenAndClose) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(1024));
  oas->Close();
}

// Test that it uses the double buffers correctly. Because it uses the actual
// audio device, you might hear a short pop noise for a short time.
TEST(WinAudioTest, DISABLED_PCMWaveStreamDoubleBuffer) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1, 16000, 16);
  ASSERT_TRUE(NULL != oas);
  TestSourceDoubleBuffer test_double_buffer;
  EXPECT_TRUE(oas->Open(512));
  oas->Start(&test_double_buffer);
  ::Sleep(300);
  EXPECT_GT(test_double_buffer.callback_count(), 2);
  EXPECT_FALSE(test_double_buffer.had_error());
  oas->Stop();
  ::Sleep(1000);
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave
// device at 44.1K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing.
TEST(WinAudioTest, DISABLED_PCMWaveStreamPlay200HzTone44Kss) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                                 AudioManager::kAudioCDSampleRate, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 10) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));
  oas->Start(&source);
  ::Sleep(1500);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for for 1.5 seconds on the default wave
// device at 22K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing.
TEST(WinAudioTest, DISABLED_PCMWaveStreamPlay200HzTone22Kss) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                                 AudioManager::kAudioCDSampleRate/2, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate/2);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 20) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));
  oas->Start(&source);
  ::Sleep(1500);
  oas->Stop();
  oas->Close();
}

