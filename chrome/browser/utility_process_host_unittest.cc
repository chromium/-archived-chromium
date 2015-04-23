// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/utility_process_host.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UtilityProcessHostTest : public testing::Test {
 public:
  UtilityProcessHostTest() {
  }

 protected:
  MessageLoopForIO message_loop_;
};

class TestUtilityProcessHostClient : public UtilityProcessHost::Client {
 public:
  explicit TestUtilityProcessHostClient(MessageLoop* message_loop)
      : message_loop_(message_loop), success_(false) {
  }

  // UtilityProcessHost::Client methods.
  virtual void OnProcessCrashed() {
    NOTREACHED();
  }

  virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest) {
    success_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  virtual void OnUnpackExtensionFailed(const std::string& error_message) {
    NOTREACHED();
  }

  virtual void OnUnpackWebResourceSucceeded(
      const ListValue& json_data) {
    NOTREACHED();
  }

  virtual void OnUnpackWebResourceFailed(
      const std::string& error_message) {
    NOTREACHED();
  }

  bool success() {
    return success_;
  }

 private:
  MessageLoop* message_loop_;
  bool success_;
};

class TestUtilityProcessHost : public UtilityProcessHost {
 public:
  TestUtilityProcessHost(TestUtilityProcessHostClient* client,
                         MessageLoop* loop_io,
                         ResourceDispatcherHost* rdh)
      : UtilityProcessHost(rdh, client, loop_io) {
  }

 protected:
  virtual std::wstring GetUtilityProcessCmd() {
    FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    exe_path = exe_path.AppendASCII(WideToASCII(
        chrome::kBrowserProcessExecutablePath));
    return exe_path.ToWStringHack();
  }

  virtual bool UseSandbox() {
    return false;
  }

 private:
};

TEST_F(UtilityProcessHostTest, ExtensionUnpacker) {
  // Copy the test extension into a temp dir and install from the temp dir.
  FilePath extension_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_file));
  extension_file = extension_file.AppendASCII("extensions")
                                 .AppendASCII("theme.crx");
  FilePath temp_extension_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &temp_extension_dir));
  temp_extension_dir = temp_extension_dir.AppendASCII("extension_test");
  ASSERT_TRUE(file_util::CreateDirectory(temp_extension_dir));
  ASSERT_TRUE(file_util::CopyFile(extension_file,
                                  temp_extension_dir.AppendASCII("theme.crx")));

  scoped_refptr<TestUtilityProcessHostClient> client(
      new TestUtilityProcessHostClient(&message_loop_));
  ResourceDispatcherHost rdh(NULL);
  TestUtilityProcessHost* process_host =
      new TestUtilityProcessHost(client.get(), &message_loop_, &rdh);
  // process_host will delete itself when it's done.
  process_host->StartExtensionUnpacker(
      temp_extension_dir.AppendASCII("theme.crx"));
  message_loop_.Run();
  EXPECT_TRUE(client->success());

  // Clean up the temp dir.
  file_util::Delete(temp_extension_dir, true);
}

}  // namespace
