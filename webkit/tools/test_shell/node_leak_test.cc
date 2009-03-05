// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

const wchar_t kTestUrlSwitch[] = L"test-url";

// A test to help determine if any nodes have been leaked as a result of
// visiting a given URL.  If enabled in WebCore, the number of leaked nodes
// can be printed upon termination.  This is only enabled in debug builds, so
// it only makes sense to run this using a debug build.
//
// It will load a URL, visit about:blank, and then perform garbage collection.
// The number of remaining (potentially leaked) nodes will be printed on exit.
class NodeLeakTest : public TestShellTest {
 public:
  virtual void SetUp() {
    const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

    std::wstring js_flags =
        parsed_command_line.GetSwitchValue(test_shell::kJavaScriptFlags);
    js_flags += L" --expose-gc";
    webkit_glue::SetJavaScriptFlags(js_flags);

    std::wstring cache_path =
        parsed_command_line.GetSwitchValue(test_shell::kCacheDir);
    if (cache_path.empty()) {
      PathService::Get(base::DIR_EXE, &cache_path);
      file_util::AppendToPath(&cache_path, L"cache");
    }

    if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
      const std::wstring timeout_str = parsed_command_line.GetSwitchValue(
          test_shell::kTestShellTimeOut);
      int timeout_ms =
          static_cast<int>(StringToInt64(WideToUTF16Hack(timeout_str.c_str())));
      if (timeout_ms > 0)
        TestShell::SetFileTestTimeout(timeout_ms);
    }

    // Optionally use playback mode (for instance if running automated tests).
    net::HttpCache::Mode mode =
        parsed_command_line.HasSwitch(test_shell::kPlaybackMode) ?
        net::HttpCache::PLAYBACK : net::HttpCache::NORMAL;
    SimpleResourceLoaderBridge::Init(
        new TestShellRequestContext(cache_path, mode, false));

    TestShellTest::SetUp();
  }

  virtual void TearDown() {
    TestShellTest::TearDown();

    SimpleResourceLoaderBridge::Shutdown();
  }

  void NavigateToURL(const std::wstring& test_url) {
    test_shell_->LoadURL(test_url.c_str());
    test_shell_->WaitTestFinished();

    // Depends on TestShellTests::TearDown to load blank page and
    // the TestShell destructor to call garbage collection.
  }
};

TEST_F(NodeLeakTest, TestURL) {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kTestUrlSwitch)) {
    NavigateToURL(parsed_command_line.GetSwitchValue(kTestUrlSwitch).c_str());
  }
}

}  // namespace
