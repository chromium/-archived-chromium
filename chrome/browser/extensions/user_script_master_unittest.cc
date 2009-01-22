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
    FilePath tmp_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &tmp_dir));
    script_dir_ = tmp_dir.AppendASCII("UserScriptTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(script_dir_, true);
    file_util::CreateDirectory(script_dir_);

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
    ASSERT_TRUE(file_util::Delete(script_dir_, true));
    ASSERT_FALSE(file_util::PathExists(script_dir_));
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

  FilePath path = script_dir_.AppendASCII("script.user.js");

  FILE* file = file_util::OpenFile(path, "w");
  const char content[] = "some content";
  fwrite(content, 1, arraysize(content), file);
  file_util::CloseFile(file);

  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

// Test that we get notified about scripts if they're already in the test dir.
TEST_F(UserScriptMasterTest, ExistingScripts) {
  FilePath path = script_dir_.AppendASCII("script.user.js");

  FILE* file = file_util::OpenFile(path, "w");
  const char content[] = "some content";
  fwrite(content, 1, arraysize(content), file);
  file_util::CloseFile(file);

  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));

  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

TEST_F(UserScriptMasterTest, Parse1) {
  const std::string text(
    "// This is my awesome script\n"
    "// It does stuff.\n"
    "// ==UserScript==   trailing garbage\n"
    "// @name foobar script\n"
    "// @namespace http://www.google.com/\n"
    "// @include *mail.google.com*\n"
    "// \n"
    "// @othergarbage\n"
    "// @include *mail.yahoo.com*\r\n"
    "// @include  \t *mail.msn.com*\n" // extra spaces after "@include" OK
    "//@include not-recognized\n" // must have one space after "//"
    "// ==/UserScript==  trailing garbage\n"
    "\n"
    "\n"
    "alert('hoo!');\n");

  std::vector<std::string> includes;
  UserScriptMaster::ScriptReloader::ParseMetadataHeader(text, &includes);
  EXPECT_EQ(3U, includes.size());
  EXPECT_EQ("*mail.google.com*", includes[0]);
  EXPECT_EQ("*mail.yahoo.com*", includes[1]);
  EXPECT_EQ("*mail.msn.com*", includes[2]);
}

TEST_F(UserScriptMasterTest, Parse2) {
  const std::string text("default to @include *");

  std::vector<std::string> includes;
  UserScriptMaster::ScriptReloader::ParseMetadataHeader(text, &includes);
  EXPECT_EQ(1U, includes.size());
  EXPECT_EQ("*", includes[0]);
}

TEST_F(UserScriptMasterTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  std::vector<std::string> includes;
  UserScriptMaster::ScriptReloader::ParseMetadataHeader(text, &includes);
  EXPECT_EQ(1U, includes.size());
  EXPECT_EQ("*foo*", includes[0]);
}
