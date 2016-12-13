// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "base/base_paths.h"
#include "base/file_util.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kAudioFile1_16b_m_16K[]
    = L"media\\test\\data\\sweep02_16b_mono_16KHz.raw";

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

bool IsRunningHeadless() {
  return (0 != ::GetEnvironmentVariableW(L"CHROME_HEADLESS", NULL, 0));
}

}  // namespace.

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

// Specializes TestSourceBasic to simulate a source that blocks for some time
// in the OnMoreData callback.
class TestSourceLaggy : public TestSourceBasic {
 public:
  TestSourceLaggy(int laggy_after_buffer, int lag_in_ms)
      : laggy_after_buffer_(laggy_after_buffer), lag_in_ms_(lag_in_ms) {
  }
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size) {
    // Call the base, which increments the callback_count_.
    TestSourceBasic::OnMoreData(stream, dest, max_size);
    if (callback_count() > 2) {
      ::Sleep(lag_in_ms_);
    }
    return max_size;
  }
 private:
  int laggy_after_buffer_;
  int lag_in_ms_;
};

// Helper class to memory map an entire file. The mapping is read-only. Don't
// use for gigabyte-sized files. Attempts to write to this memory generate
// memory access violations.
class ReadOnlyMappedFile {
 public:
  ReadOnlyMappedFile(const wchar_t* file_name)
      : fmap_(NULL), start_(NULL), size_(0) {
    HANDLE file = ::CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == file)
      return;
    fmap_ = ::CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    ::CloseHandle(file);
    if (!fmap_)
      return;
    start_ = reinterpret_cast<char*>(::MapViewOfFile(fmap_, FILE_MAP_READ,
                                                     0, 0, 0));
    if (!start_)
      return;
    MEMORY_BASIC_INFORMATION mbi = {0};
    ::VirtualQuery(start_, &mbi, sizeof(mbi));
    size_ = mbi.RegionSize;
  }
  ~ReadOnlyMappedFile() {
    if (start_) {
      ::UnmapViewOfFile(start_);
      ::CloseHandle(fmap_);
    }
  }
  // Returns true if the file was successfully mapped.
  bool is_valid() const {
    return ((start_ > 0) && (size_ > 0));
  }
  // Returns the size in bytes of the mapped memory.
  size_t size() const {
    return size_;
  }
  // Returns the memory backing the file.
  const void* GetChunkAt(size_t offset) {
    return &start_[offset];
  }

 private:
  HANDLE fmap_;
  char* start_;
  size_t size_;
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
// TODO(hclam): move this test to SimpleSourcesTest once mock audio stream is
// implemented on other platform.
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
//
// The tests tend to fail in the build bots when somebody connects to them via
// via remote-desktop because it installs an audio device that fails to open
// at some point, possibly when the connection goes idle. So that is why we
// skipped them in headless mode.

// Test that can it be created and closed.
TEST(WinAudioTest, PCMWaveStreamGetAndClose) {
  if (IsRunningHeadless())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that can it be cannot be created with crazy parameters
TEST(WinAudioTest, SanityOnMakeParams) {
  if (IsRunningHeadless())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioManager::Format fmt = AudioManager::AUDIO_PCM_LINEAR;
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, 8, 8000, 16));
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, 1, 1024 * 1024, 16));
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, 2, 8000, 80));
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, -2, 8000, 16));
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, 2, -8000, 16));
  EXPECT_TRUE(NULL == audio_man->MakeAudioStream(fmt, 2, -8000, -16));
}

// Test that it can be opened and closed.
TEST(WinAudioTest, PCMWaveStreamOpenAndClose) {
  if (IsRunningHeadless())
    return;
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

// Test that it has a maximum packet size.
TEST(WinAudioTest, PCMWaveStreamOpenLimit) {
  if (IsRunningHeadless())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  EXPECT_FALSE(oas->Open(1024 * 1024 * 1024));
  oas->Close();
}

// Test that it uses the double buffers correctly. Because it uses the actual
// audio device, you might hear a short pop noise for a short time.
TEST(WinAudioTest, PCMWaveStreamDoubleBuffer) {
  if (IsRunningHeadless())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
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

// Test potential deadlock situations if the source is slow or blocks for some
// time. The actual EXPECT_GT are mostly meaningless and the real test is that
// the test completes in reasonable time.
TEST(WinAudioTest, PCMWaveSlowSource) {
  if (IsRunningHeadless())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1, 16000, 16);
  ASSERT_TRUE(NULL != oas);
  TestSourceLaggy test_laggy(2, 90);
  EXPECT_TRUE(oas->Open(512));
  // The test parameters cause a callback every 32 ms and the source is
  // sleeping for 90 ms, so it is guaranteed that we run out of ready buffers.
  oas->Start(&test_laggy);
  ::Sleep(1000);
  EXPECT_GT(test_laggy.callback_count(), 2);
  EXPECT_FALSE(test_laggy.had_error());
  oas->Stop();
  ::Sleep(1000);
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave
// device at 44.1K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing.
TEST(WinAudioTest, PCMWaveStreamPlay200HzTone44Kss) {
  if (IsRunningHeadless())
    return;
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
  oas->SetVolume(1.0, 1.0);
  oas->Start(&source);
  ::Sleep(1500);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for for 1.5 seconds on the default wave
// device at 22K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing. The audio also should
// sound with a lower volume than PCMWaveStreamPlay200HzTone44Kss.
TEST(WinAudioTest, PCMWaveStreamPlay200HzTone22Kss) {
  if (IsRunningHeadless())
    return;
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

  oas->SetVolume(0.5, 0.5);
  oas->Start(&source);
  ::Sleep(1500);

  // Test that the volume is within the set limits.
  double left_volume = 0.0;
  double right_volume = 0.0;
  oas->GetVolume(&left_volume, &right_volume);
  EXPECT_LT(left_volume, 0.51);
  EXPECT_GT(left_volume, 0.49);
  EXPECT_LT(right_volume, 0.51);
  EXPECT_GT(right_volume, 0.49);
  oas->Stop();
  oas->Close();
}

// Uses the PushSource to play a 2 seconds file clip for about 5 seconds. We
// try hard to generate situation where the two threads are accessing the
// object roughly at the same time. What you hear is a sweeping tone from 1KHz
// to 2KHz with a bit of fade out at the end for one second. The file is two
// of these sweeping tones back to back.
TEST(WinAudioTest, PushSourceFile16KHz)  {
  if (IsRunningHeadless())
    return;
  // Open sweep02_16b_mono_16KHz.raw which has no format. It contains the
  // raw 16 bit samples for a single channel in little-endian format. The
  // creation sample rate is 16KHz.
  FilePath audio_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &audio_file));
  audio_file = audio_file.Append(kAudioFile1_16b_m_16K);
  // Map the entire file in memory.
  ReadOnlyMappedFile file_reader(audio_file.value().c_str());
  ASSERT_TRUE(file_reader.is_valid());

  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1, 16000, 16);
  ASSERT_TRUE(NULL != oas);

  // compute buffer size for 100ms of audio. Which is 3200 bytes.
  const size_t kSize50ms = 2 * (16000 / 1000) * 100;
  EXPECT_TRUE(oas->Open(kSize50ms));

  size_t offset = 0;
  const size_t kMaxStartOffset = file_reader.size() - kSize50ms;

  // We buffer and play at the same time, buffering happens every ~10ms and the
  // consuming of the buffer happens every ~50ms. We do 100 buffers which
  // effectively wrap around the file more than once.
  PushSource push_source(kSize50ms);
  for (size_t ix = 0; ix != 100; ++ix) {
    push_source.Write(file_reader.GetChunkAt(offset), kSize50ms);
    if (ix == 2) {
      // For glitch free, start playing after some buffers are in.
      oas->Start(&push_source);
    }
    ::Sleep(10);
    offset += kSize50ms;
    if (offset > kMaxStartOffset)
      offset = 0;
  }

  // Play a little bit more of the file.
  ::Sleep(4000);

  oas->Stop();
  oas->Close();
}

// This test is to make sure an AudioOutputStream can be started after it was
// stopped. You will here two 1.5 seconds wave signal separated by 0.5 seconds
// of silence.
TEST(WinAudioTest, PCMWaveStreamPlayTwice200HzTone44Kss) {
  if (IsRunningHeadless())
    return;
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
  oas->SetVolume(1.0, 1.0);

  // Play the wave for 1.5 seconds.
  oas->Start(&source);
  ::Sleep(1500);
  oas->Stop();

  // Sleep to give silence after stopping the AudioOutputStream.
  ::Sleep(500);

  // Start again and play for 1.5 seconds.
  oas->Start(&source);
  ::Sleep(1500);
  oas->Stop();

  oas->Close();
}

