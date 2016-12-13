// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/file_system_accessor.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class FileSystemAccessorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Make sure the current thread has a message loop.
    EXPECT_TRUE(MessageLoop::current() != NULL);

    // Create FILE thread which is used to do file access.
    file_thread_.reset(new ChromeThread(ChromeThread::FILE));
    EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);

    // Start FILE thread and verify FILE message loop exists.
    file_thread_->Start();
    EXPECT_TRUE(file_thread_->message_loop() != NULL);
  }

  virtual void TearDown() {
    file_thread_->Stop();
    EXPECT_TRUE(ChromeThread::GetMessageLoop(ChromeThread::FILE) == NULL);
  }

  void TestGetFileSize(const FilePath& path,
                       void* param,
                       int64 expected_result,
                       void* expected_param) {
    // Initialize the actual result not equal to the expected result.
    result_ = expected_result + 1;
    param_ = NULL;

    FileSystemAccessor::RequestFileSize(
        path,
        param,
        NewCallback(this, &FileSystemAccessorTest::GetFileSizeCallback));

    // Time out if getting file size takes more than 10 seconds.
    const int kGetFileSizeTimeoutSeconds = 10;
    base::OneShotTimer<MessageLoop> timer;
    timer.Start(base::TimeDelta::FromSeconds(kGetFileSizeTimeoutSeconds),
                MessageLoop::current(), &MessageLoop::Quit);

    MessageLoop::current()->Run();

    EXPECT_EQ(expected_result, result_);
    EXPECT_EQ(expected_param, param_);
  }

 private:
  void GetFileSizeCallback(int64 result, void* param) {
    result_ = result;
    param_ = param;
    MessageLoop::current()->Quit();
  }

  scoped_ptr<ChromeThread> file_thread_;
  FilePath temp_file_;
  int64 result_;
  void* param_;
  MessageLoop loop_;
};

TEST_F(FileSystemAccessorTest, GetFileSize) {
  const std::string data("This is test data.");

  FilePath path;
  FILE* file = file_util::CreateAndOpenTemporaryFile(&path);
  EXPECT_TRUE(file != NULL);
  size_t bytes_written = fwrite(data.data(), 1, data.length(), file);
  EXPECT_TRUE(file_util::CloseFile(file));

  FileAutoDeleter deleter(path);

  TestGetFileSize(path, NULL, bytes_written, NULL);
}

TEST_F(FileSystemAccessorTest, GetFileSizeWithParam) {
  const std::string data("This is test data.");

  FilePath path;
  FILE* file = file_util::CreateAndOpenTemporaryFile(&path);
  EXPECT_TRUE(file != NULL);
  size_t bytes_written = fwrite(data.data(), 1, data.length(), file);
  EXPECT_TRUE(file_util::CloseFile(file));

  FileAutoDeleter deleter(path);

  int param = 100;
  TestGetFileSize(path, static_cast<void*>(&param),
                  bytes_written, static_cast<void*>(&param));
}

TEST_F(FileSystemAccessorTest, GetFileSizeEmptyFile) {
  FilePath path;
  EXPECT_TRUE(file_util::CreateTemporaryFileName(&path));
  FileAutoDeleter deleter(path);

  TestGetFileSize(path, NULL, 0, NULL);
}

TEST_F(FileSystemAccessorTest, GetFileSizeNotFound) {
  FilePath path;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_test_"), &path));
  FileAutoDeleter deleter(path);

  TestGetFileSize(path.Append(FILE_PATH_LITERAL("foo.txt")), NULL, -1, NULL);
}
