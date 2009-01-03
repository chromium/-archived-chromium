// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <fstream>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// For tests where we wait a bit to verify nothing happened
namespace {
const int kWaitForEventTime = 500;
}

class DirectoryWatcherTest : public testing::Test,
                             public DirectoryWatcher::Delegate {
 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    std::wstring path_str;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_str));
    test_dir_ = FilePath(path_str).Append(
        FILE_PATH_LITERAL("DirectoryWatcherTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_.value(), true);
    file_util::CreateDirectory(test_dir_.value());

    directory_mods_ = 0;
    quit_mod_count_ = 0;
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    EXPECT_EQ(path.value(), test_dir_.value());
    ++directory_mods_;
    if (directory_mods_ == quit_mod_count_)
      MessageLoop::current()->Quit();
  }

  virtual void TearDown() {
    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(test_dir_.value(), true));
    ASSERT_FALSE(file_util::PathExists(test_dir_.value()));
  }

  // Write |content| to a file under the test directory.
  void WriteTestDirFile(const FilePath::StringType& filename,
                        const std::string& content) {
    EXPECT_FALSE(FilePath(filename).IsAbsolute());
    FilePath path = test_dir_.Append(filename);

    std::ofstream file;
    file.open(WideToUTF8(path.value()).c_str());
    file << content;
    file.close();
  }

  // Run the message loop until we've seen |n| directory modifications.
  void LoopUntilModsEqual(int n) {
    quit_mod_count_ = n;
    loop_.Run();
  }

  MessageLoop loop_;

  // The path to a temporary directory used for testing.
  FilePath test_dir_;

  // The number of times a directory modification has been observed.
  int directory_mods_;

  // The number of directory mods which, when reached, cause us to quit
  // our message loop.
  int quit_mod_count_;
};

// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NewFile) {
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this));

  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  LoopUntilModsEqual(2);
}

// Verify that modifying a file is caught.
TEST_F(DirectoryWatcherTest, ModifiedFile) {
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this));

  // Write a file to the test dir.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  LoopUntilModsEqual(2);

  // Now make sure we get notified if the file is modified.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some new content");
  LoopUntilModsEqual(3);
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(DirectoryWatcherTest, Unregister) {
  {
    DirectoryWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_dir_, this));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");

  // We won't get a notification, so we just wait around a bit to verify
  // that notification doesn't come.
  loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                        kWaitForEventTime);
  loop_.Run();

  ASSERT_EQ(directory_mods_, 0);
}

// Verify that modifications to a subdirectory isn't noticed.
TEST_F(DirectoryWatcherTest, SubDir) {
  FilePath subdir(FILE_PATH_LITERAL("SubDir"));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdir)));

  // Create a file in the subdir.
  FilePath test_path = subdir.Append(FILE_PATH_LITERAL("test_file"));
  WriteTestDirFile(test_path.value(), "some content");

#if defined(OS_WIN)
  // We will flush the write buffer now to ensure it won't interfere
  // with the notifications.
  FilePath full_path(test_dir_.Append(test_path));
  HANDLE file_handle = CreateFile(full_path.value().c_str(),
                                  GENERIC_READ | GENERIC_WRITE,
                                  0, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
  ASSERT_TRUE(file_handle != INVALID_HANDLE_VALUE);
  ASSERT_TRUE(FlushFileBuffers(file_handle));
  CloseHandle(file_handle);
#endif

  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this));

  WriteTestDirFile(test_path.value(), "changed content");

  // We won't get a notification, so we just wait around a bit to verify
  // that notification doesn't come.
  loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                        kWaitForEventTime);
  loop_.Run();

  // We shouldn't have been notified and shouldn't have crashed.
  ASSERT_EQ(0, directory_mods_);
}

namespace {
// Used by the DeleteDuringNotify test below.
// Deletes the DirectoryWatcher when it's notified.
class Deleter : public DirectoryWatcher::Delegate {
 public:
  Deleter(DirectoryWatcher* watcher) : watcher_(watcher) {}
  virtual void OnDirectoryChanged(const FilePath& path) {
    watcher_.reset(NULL);
    MessageLoop::current()->Quit();
  }

  scoped_ptr<DirectoryWatcher> watcher_;
};
}  // anonymous namespace

// Verify that deleting a watcher during the callback
TEST_F(DirectoryWatcherTest, DeleteDuringNotify) {
  DirectoryWatcher* watcher = new DirectoryWatcher;
  Deleter deleter(watcher);  // Takes ownership of watcher.
  ASSERT_TRUE(watcher->Watch(test_dir_, &deleter));

  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  LoopUntilModsEqual(2);

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher_.get() == NULL);
}
