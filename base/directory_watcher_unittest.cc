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
#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// For tests where we wait a bit to verify nothing happened
const int kWaitForEventTime = 1000;

class DirectoryWatcherTest : public testing::Test {
 public:
  // Implementation of DirectoryWatcher on Mac requires UI loop.
  DirectoryWatcherTest() : loop_(MessageLoop::TYPE_UI) {
  }

  void OnTestDelegateFirstNotification(const FilePath& path) {
    notified_delegates_++;
    if (notified_delegates_ >= expected_notified_delegates_)
      MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    FilePath path;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path));
    test_dir_ = path.Append(FILE_PATH_LITERAL("DirectoryWatcherTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
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

  void SetExpectedNumberOfNotifiedDelegates(int n) {
    notified_delegates_ = 0;
    expected_notified_delegates_ = n;
  }

  void VerifyExpectedNumberOfNotifiedDelegates() {
    // Check that we get at least the expected number of notified delegates.
    if (expected_notified_delegates_ - notified_delegates_ > 0)
      loop_.Run();

    // Check that we get no more than the expected number of notified delegates.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    EXPECT_EQ(expected_notified_delegates_, notified_delegates_);
  }

  // We need this function for reliable tests on Mac OS X. FSEvents API
  // has a latency interval and can merge multiple events into one,
  // and we need a clear distinction between events triggered by test setup code
  // and test code.
  void SyncIfPOSIX() {
#if defined(OS_POSIX)
    sync();
#endif  // defined(OS_POSIX)
  }

  MessageLoop loop_;

  // The path to a temporary directory used for testing.
  FilePath test_dir_;

  // The number of test delegates which received their notification.
  int notified_delegates_;

  // The number of notified test delegates after which we quit the message loop.
  int expected_notified_delegates_;
};

class TestDelegate : public DirectoryWatcher::Delegate {
 public:
  TestDelegate(DirectoryWatcherTest* test)
      : test_(test),
        got_notification_(false),
        original_thread_id_(PlatformThread::CurrentId()) {
  }

  bool got_notification() const {
    return got_notification_;
  }

  void reset() {
    got_notification_ = false;
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    EXPECT_EQ(original_thread_id_, PlatformThread::CurrentId());
    if (!got_notification_)
      test_->OnTestDelegateFirstNotification(path);
    got_notification_ = true;
  }

 private:
  // Hold a pointer to current test fixture to inform it on first notification.
  DirectoryWatcherTest* test_;

  // Set to true after first notification.
  bool got_notification_;

  // Keep track of original thread id to verify that callbacks are called
  // on the same thread.
  PlatformThreadId original_thread_id_;
};

// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NewFile) {
  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  SetExpectedNumberOfNotifiedDelegates(1);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that modifying a file is caught.
TEST_F(DirectoryWatcherTest, ModifiedFile) {
  // Write a file to the test dir.
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");

  SyncIfPOSIX();

  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  // Now make sure we get notified if the file is modified.
  SetExpectedNumberOfNotifiedDelegates(1);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some new content");
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(DirectoryWatcherTest, Unregister) {
  TestDelegate delegate(this);

  {
    DirectoryWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  SetExpectedNumberOfNotifiedDelegates(0);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, SubDirRecursive) {
  FilePath subdir(FILE_PATH_LITERAL("SubDir"));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdir)));

#if defined(OS_LINUX)
  // TODO(port): Recursive watches are not implemented on Linux.
  return;
#endif  // !defined(OS_WIN)

  SyncIfPOSIX();

  // Verify that modifications to a subdirectory are noticed by recursive watch.
  TestDelegate delegate(this);
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, true));
  // Write a file to the subdir.
  SetExpectedNumberOfNotifiedDelegates(1);
  FilePath test_path = subdir.AppendASCII("test_file");
  WriteTestDirFile(test_path.value(), "some content");
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, SubDirNonRecursive) {
#if defined(OS_WIN)
  // Disable this test for earlier version of Windows. It turned out to be
  // very difficult to create a reliable test for them.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)

  FilePath subdir(FILE_PATH_LITERAL("SubDir"));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdir)));

  // Create a test file before the test. On Windows we get a notification
  // when creating a file in a subdir even with a non-recursive watch.
  FilePath test_path = subdir.AppendASCII("test_file");
  WriteTestDirFile(test_path.value(), "some content");

  SyncIfPOSIX();

  // Verify that modifications to a subdirectory are not noticed
  // by a not-recursive watch.
  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  // Modify the test file. There should be no notifications.
  SetExpectedNumberOfNotifiedDelegates(0);
  WriteTestDirFile(test_path.value(), "some other content");
  VerifyExpectedNumberOfNotifiedDelegates();
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
  TestDelegate delegate1(this), delegate2(this);
  ASSERT_TRUE(watcher1.Watch(test_dir_, &delegate1, false));
  ASSERT_TRUE(watcher2.Watch(test_dir_, &delegate2, false));

  SetExpectedNumberOfNotifiedDelegates(2);
  WriteTestDirFile(FILE_PATH_LITERAL("test_file"), "some content");
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, MultipleWatchersDifferentFiles) {
  const int kNumberOfWatchers = 5;
  DirectoryWatcher watchers[kNumberOfWatchers];
  TestDelegate delegates[kNumberOfWatchers] = {this, this, this, this, this};
  FilePath subdirs[kNumberOfWatchers];
  for (int i = 0; i < kNumberOfWatchers; i++) {
    subdirs[i] = FilePath(FILE_PATH_LITERAL("Dir")).AppendASCII(IntToString(i));
    ASSERT_TRUE(file_util::CreateDirectory(test_dir_.Append(subdirs[i])));

    ASSERT_TRUE(watchers[i].Watch(test_dir_.Append(subdirs[i]), &delegates[i],
                                  false));
  }
  for (int i = 0; i < kNumberOfWatchers; i++) {
    // Verify that we only get modifications from one watcher (each watcher has
    // different directory).

    for (int j = 0; j < kNumberOfWatchers; j++)
      delegates[j].reset();

    // Write a file to the subdir.
    FilePath test_path = subdirs[i].AppendASCII("test_file");
    SetExpectedNumberOfNotifiedDelegates(1);
    WriteTestDirFile(test_path.value(), "some content");
    VerifyExpectedNumberOfNotifiedDelegates();

    loop_.RunAllPending();
  }
}

// Verify that watching a directory that doesn't exist fails, but doesn't
// asssert.
// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NonExistentDirectory) {
  DirectoryWatcher watcher;
  ASSERT_FALSE(watcher.Watch(test_dir_.AppendASCII("does-not-exist"), NULL,
                             false));
}

}  // namespace
