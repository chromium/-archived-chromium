// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output.h"
#include "testing/gtest/include/gtest/gtest.h"

class SimpleSource8bitStereo : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit SimpleSource8bitStereo(size_t bytes_to_write)
      : byte_count_(bytes_to_write), callback_count_(0) {
  }

  // AudioSourceCallback::OnMoreData implementation:
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size) {
    ++callback_count_;
    return byte_count_;
  }

  // AudioSourceCallback::OnClose implementation:
  virtual void OnClose(AudioOutputStream* stream) {
  }

  // AudioSourceCallback::OnError implementation:
  virtual void OnError(AudioOutputStream* stream, int code) {
  }

  // Returns how many times OnMoreData() has been called.
  int callback_count() const {
    return callback_count_;
  }

 private:
  size_t byte_count_;
  int callback_count_;
};

// Validate that the AudioManager::AUDIO_MOCK works.
TEST(WinAudioTest, MockStream) {
  AudioManager* audio_man = GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_MOCK, 2, 8000, 8);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(256));
  SimpleSource8bitStereo source(256);
  oas->Start(&source);
  EXPECT_GT(source.callback_count(), 0);
  oas->Close();
}


