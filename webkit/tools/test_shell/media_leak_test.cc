// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

class MediaLeakTest : public TestShellTest {
};

// <video> and <audio> tags only work stably on Windows.
#if defined(OS_WIN)

// This test is to be executed in test_shell_tests so we can capture memory
// leak analysis in automated runs.
TEST_F(MediaLeakTest, VideoBear) {
  FilePath media_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &media_file));
  media_file = media_file.Append(FILE_PATH_LITERAL("webkit"))
                         .Append(FILE_PATH_LITERAL("data"))
                         .Append(FILE_PATH_LITERAL("media"))
                         .Append(FILE_PATH_LITERAL("bear.html"));
  test_shell_->LoadURL(media_file.ToWStringHack().c_str());
  test_shell_->WaitTestFinished();
}

#endif
