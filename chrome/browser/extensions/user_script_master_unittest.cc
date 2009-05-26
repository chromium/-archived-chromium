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
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test bringing up a master on a specific directory, putting a script
// in there, etc.

class UserScriptMasterTest : public testing::Test,
                             public NotificationObserver {
 public:
  UserScriptMasterTest()
      : message_loop_(MessageLoop::TYPE_UI),
        shared_memory_(NULL) {
  }

  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    FilePath tmp_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &tmp_dir));
    script_dir_ = tmp_dir.AppendASCII("UserScriptTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(script_dir_, true);
    file_util::CreateDirectory(script_dir_);

    // Register for all user script notifications.
    registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                   NotificationService::AllSources());
  }

  virtual void TearDown() {
    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(script_dir_, true));
    ASSERT_FALSE(file_util::PathExists(script_dir_));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::USER_SCRIPTS_UPDATED);

    shared_memory_ = Details<base::SharedMemory>(details).ptr();
    if (MessageLoop::current() == &message_loop_)
      MessageLoop::current()->Quit();
  }

  NotificationRegistrar registrar_;

  // MessageLoop used in tests.
  MessageLoop message_loop_;

  // Directory containing user scripts.
  FilePath script_dir_;

  // Updated to the script shared memory when we get notified.
  base::SharedMemory* shared_memory_;
};

// Test that we get notified even when there are no scripts.
TEST_F(UserScriptMasterTest, NoScripts) {

  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));
  master->StartScan();
  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

// TODO(shess): Disabled on Linux because of missing DirectoryWatcher.
#if defined(OS_WIN) || defined(OS_MACOSX)
// Test that we get notified about new scripts after they're added.
TEST_F(UserScriptMasterTest, NewScripts) {
  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));

  FilePath path = script_dir_.AppendASCII("script.user.js");

  const char content[] = "some content";
  size_t written = file_util::WriteFile(path, content, sizeof(content));
  ASSERT_EQ(written, sizeof(content));

  // Post a delayed task so that we fail rather than hanging if things
  // don't work.
  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask, 5000);

  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}
#endif

// Test that we get notified about scripts if they're already in the test dir.
TEST_F(UserScriptMasterTest, ExistingScripts) {
  FilePath path = script_dir_.AppendASCII("script.user.js");

  const char content[] = "some content";
  size_t written = file_util::WriteFile(path, content, sizeof(content));
  ASSERT_EQ(written, sizeof(content));

  scoped_refptr<UserScriptMaster> master(
      new UserScriptMaster(MessageLoop::current(), script_dir_));
  master->StartScan();

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

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  EXPECT_EQ(3U, script.globs().size());
  EXPECT_EQ("*mail.google.com*", script.globs()[0]);
  EXPECT_EQ("*mail.yahoo.com*", script.globs()[1]);
  EXPECT_EQ("*mail.msn.com*", script.globs()[2]);
}

TEST_F(UserScriptMasterTest, Parse2) {
  const std::string text("default to @include *");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  EXPECT_EQ(1U, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
}

TEST_F(UserScriptMasterTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  UserScript script;
  UserScriptMaster::ScriptReloader::ParseMetadataHeader(text, &script);
  EXPECT_EQ(1U, script.globs().size());
  EXPECT_EQ("*foo*", script.globs()[0]);
}

TEST_F(UserScriptMasterTest, Parse4) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  EXPECT_EQ(0U, script.globs().size());
  EXPECT_EQ(2U, script.url_patterns().size());
  EXPECT_EQ("http://*.mail.google.com/*",
            script.url_patterns()[0].GetAsString());
  EXPECT_EQ("http://mail.yahoo.com/*",
            script.url_patterns()[1].GetAsString());
}

TEST_F(UserScriptMasterTest, Parse5) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*mail.google.com/*\n"
    "// ==/UserScript==\n");

  // Invalid @match value.
  UserScript script;
  EXPECT_FALSE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
}

TEST_F(UserScriptMasterTest, Parse6) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  // Not allowed to mix @include and @value.
  UserScript script;
  EXPECT_FALSE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
}
