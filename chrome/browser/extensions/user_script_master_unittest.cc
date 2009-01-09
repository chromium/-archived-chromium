// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <fstream>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test bringing up a master on a specific directory, putting a script in there, etc.

class UserScriptMasterTest : public testing::Test,
                             public NotificationObserver {
 public:
  UserScriptMasterTest() : shared_memory_(NULL) {}

  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    std::wstring path_str;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_str));
    script_dir_ = FilePath(path_str).Append(
        FILE_PATH_LITERAL("UserScriptTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(script_dir_.value(), true);
    file_util::CreateDirectory(script_dir_.value());

    // Register for all user script notifications.
    NotificationService::current()->AddObserver(this,
        NOTIFY_USER_SCRIPTS_LOADED,
        NotificationService::AllSources());
  }

  virtual void TearDown() {
    NotificationService::current()->RemoveObserver(this,
        NOTIFY_USER_SCRIPTS_LOADED,
        NotificationService::AllSources());

    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(script_dir_.value(), true));
    ASSERT_FALSE(file_util::PathExists(script_dir_.value()));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NOTIFY_USER_SCRIPTS_LOADED);

    shared_memory_ = Details<base::SharedMemory>(details).ptr();
    if (MessageLoop::current() == &message_loop_)
      MessageLoop::current()->Quit();
  }

  // MessageLoop used in tests.
  MessageLoop message_loop_;

  // Directory containing user scripts.
  FilePath script_dir_;

  // Updated to the script shared memory when we get notified.
  base::SharedMemory* shared_memory_;
};

// Test that we *don't* get spurious notifications.
TEST_F(UserScriptMasterTest, NoScripts) {
  // Set shared_memory_ to something non-NULL, so we can check it became NULL.
  shared_memory_ = reinterpret_cast<base::SharedMemory*>(1);

  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));
  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  // There were no scripts in the script dir, so we shouldn't have gotten
  // a notification.
  ASSERT_EQ(NULL, shared_memory_);
}

// Test that we get notified about new scripts after they're added.
TEST_F(UserScriptMasterTest, NewScripts) {
  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));

  FilePath path = script_dir_.Append(FILE_PATH_LITERAL("script.user.js"));

  std::ofstream file;
  file.open(WideToUTF8(path.value()).c_str());
  file << "some content";
  file.close();

  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

// Test that we get notified about scripts if they're already in the test dir.
TEST_F(UserScriptMasterTest, ExistingScripts) {
  FilePath path = script_dir_.Append(FILE_PATH_LITERAL("script.user.js"));
  std::ofstream file;
  file.open(WideToUTF8(path.value()).c_str());
  file << "some content";
  file.close();

  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));

  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}
