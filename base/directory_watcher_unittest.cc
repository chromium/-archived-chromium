// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <limits>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// For tests where we wait a bit to verify nothing happened
const int kWaitForEventTime = 500;

}  // namespace

class DirectoryWatcherTest : public testing::Test,
                             public DirectoryWatcher::Delegate {
 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    FilePath path;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path));
    test_dir_ = path.Append(FILE_PATH_LITERAL("DirectoryWatcherTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);

    directory_mods_ = 0;
    quit_mod_count_ = 0;

    original_thread_id_ = PlatformThread::CurrentId();
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    EXPECT_EQ(original_thread_id_, PlatformThread::CurrentId());
    ++directory_mods_;
    // The exact number is verified by VerifyExpectedNumberOfModifications.
    // Sometimes we don't want such check, see WaitForFirstNotification.
    if (directory_mods_ >= quit_mod_count_)
      MessageLoop::current()->Quit();
  }

  virtual void TearDown() {
    // Make sure there are no tasks in the loop.
    loop_.RunAllPending();

    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // Write |content| to a file under the test directory.
  void WriteTestDirFile(const FilePath::StringType& filename,
                        const std::string& content) {
    FilePath path = test_dir_.Append(filename);
    file_util::WriteFile(path, content.c_str(), content.length());
  }

  void SetExpectedNumberOfModifications(int n) {
    quit_mod_count_ = n;
  }

  void VerifyExpectedNumberOfModifications() {
    // Check that we get at least the expected number of notifications.
    if (quit_mod_count_ - directory_mods_ > 0)
      loop_.Run();

    // Check that we get no more than the expected number of notifications.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    EXPECT_EQ(quit_mod_count_, directory_mods_);
  }

  // Only use this function if you don't care about getting
  // too many notifications. Useful for tests where you get
  // different number of notifications on different platforms.
  void WaitForFirstNotification() {
    directory_mods_ = 0;
    SetExpectedNumberOfModifications(1);
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

  // Keep track of original thread id to verify that callbacks are called
  // on the same thread.
  PlatformThreadId original_thread_id_;
};

// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NewFile) {
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this, false));

  SetExpectedNumberOfModifications(2);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfModifications();
}

// Verify that modifying a file is caught.
TEST_F(DirectoryWatcherTest, ModifiedFile) {
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this, false));

  // Write a file to the test dir.
  SetExpectedNumberOfModifications(2);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfModifications();

  // Now make sure we get notified if the file is modified.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some new content");
  // Use a more forgiving function to check because on Linux you will get
  // 1 notification, and on Windows 2 (and nothing seems to convince it to
  // send less notifications).
  WaitForFirstNotification();
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(DirectoryWatcherTest, Unregister) {
  {
    DirectoryWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_dir_, this, false));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  SetExpectedNumberOfModifications(0);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfModifications();
}

TEST_F(DirectoryWatcherTest, SubDirRecursive) {
  FilePath subdir(FILE_PATH_LITERAL("SubDir"));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdir)));

#if !defined(OS_WIN)
  // TODO(port): Recursive watches are not implemented on Linux.
  return;
#endif  // !defined(OS_WIN)

  // Verify that modifications to a subdirectory are noticed by recursive watch.
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this, true));
  // Write a file to the subdir.
  SetExpectedNumberOfModifications(2);
  FilePath test_path = subdir.AppendASCII("test_file");
  WriteTestDirFile(test_path.value(), "some content");
  VerifyExpectedNumberOfModifications();
}

TEST_F(DirectoryWatcherTest, SubDirNonRecursive) {
  FilePath subdir(FILE_PATH_LITERAL("SubDir"));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdir)));

  // Create a test file before the test. On Windows we get a notification
  // when creating a file in a subdir even with a non-recursive watch.
  FilePath test_path = subdir.AppendASCII("test_file");
  WriteTestDirFile(test_path.value(), "some content");

  // Verify that modifications to a subdirectory are not noticed
  // by a not-recursive watch.
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, this, false));

  // Modify the test file. There should be no notifications.
  SetExpectedNumberOfModifications(0);
  WriteTestDirFile(test_path.value(), "some other content");
  VerifyExpectedNumberOfModifications();
}

namespace {
// Used by the DeleteDuringNotify test below.
// Deletes the DirectoryWatcher when it's notified.
class Deleter : public DirectoryWatcher::Delegate {
 public:
  Deleter(DirectoryWatcher* watcher, MessageLoop* loop)
      : watcher_(watcher),
        loop_(loop) {
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    watcher_.reset(NULL);
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  scoped_ptr<DirectoryWatcher> watcher_;
  MessageLoop* loop_;
};
}  // anonymous namespace

// Verify that deleting a watcher during the callback
TEST_F(DirectoryWatcherTest, DeleteDuringNotify) {
  DirectoryWatcher* watcher = new DirectoryWatcher;
  Deleter deleter(watcher, &loop_);  // Takes ownership of watcher.
  ASSERT_TRUE(watcher->Watch(test_dir_, &deleter, false));

  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  loop_.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher_.get() == NULL);
}

TEST_F(DirectoryWatcherTest, MultipleWatchersSingleFile) {
  DirectoryWatcher watcher1, watcher2;
  ASSERT_TRUE(watcher1.Watch(test_dir_, this, false));
  ASSERT_TRUE(watcher2.Watch(test_dir_, this, false));

  SetExpectedNumberOfModifications(4);  // Each watcher should fire twice.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfModifications();
}

TEST_F(DirectoryWatcherTest, MultipleWatchersDifferentFiles) {
  const int kNumberOfWatchers = 5;
  DirectoryWatcher watchers[kNumberOfWatchers];
  FilePath subdirs[kNumberOfWatchers];
  for (int i = 0; i < kNumberOfWatchers; i++) {
    subdirs[i] = FilePath(FILE_PATH_LITERAL("Dir")).AppendASCII(IntToString(i));
    ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdirs[i])));

    ASSERT_TRUE(watchers[i].Watch(test_dir_.Append(subdirs[i]), this, false));
  }
  for (int i = 0; i < kNumberOfWatchers; i++) {
    // Verify that we only get modifications from one watcher (each watcher has
    // different directory).

    ASSERT_EQ(0, directory_mods_);

    // Write a file to the subdir.
    FilePath test_path = subdirs[i].AppendASCII("test_file");
    SetExpectedNumberOfModifications(2);
    WriteTestDirFile(test_path.value(), "some content");
    VerifyExpectedNumberOfModifications();

    directory_mods_ = 0;
  }
}

// Verify that watching a directory that doesn't exist fails, but doesn't
// asssert.
// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NonExistentDirectory) {
  DirectoryWatcher watcher;
  ASSERT_FALSE(watcher.Watch(test_dir_.AppendASCII("does-not-exist"), this,
                             false));
}
