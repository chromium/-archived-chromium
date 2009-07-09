// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class MediaTest : public UITest {
 protected:
  void PlayMedia(const char* tag, const char* media_file) {
    FilePath test_file(test_data_directory_);
    test_file = test_file.AppendASCII("/media/player.html");

    GURL player_gurl = net::FilePathToFileURL(test_file);
    std::string url = StringPrintf("%s?%s=%s",
                                   player_gurl.spec().c_str(),
                                   tag,
                                   media_file);

    NavigateToURL(GURL(url));

    // Allow the media file to be loaded.
    const std::wstring kPlaying = L"PLAYING";
    const std::wstring kFailed = L"FAILED";
    const std::wstring kError = L"ERROR";
    for (int i = 0; i < 10; ++i) {
      PlatformThread::Sleep(sleep_timeout_ms());
      const std::wstring& title = GetActiveTabTitle();
      if (title == kPlaying || title == kFailed ||
          StartsWith(title, kError, true))
        break;
    }

    EXPECT_EQ(kPlaying, GetActiveTabTitle());
  }

  void PlayAudio(const char* url) {
    PlayMedia("audio", url);
  }

  void PlayVideo(const char* url) {
    PlayMedia("video", url);
  }
};

TEST_F(MediaTest, DISABLED_VideoBearH264) {
  PlayVideo("bear.mp4");
}
