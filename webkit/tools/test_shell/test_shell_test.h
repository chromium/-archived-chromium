// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base test class used by all test shell tests.  Provides boiler plate
 * code to create and destroy a new test shell for each gTest test.
 */

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__

#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestShellTest : public testing::Test {
 protected:
  // Returns the path "test_case_path/test_case".
  std::wstring GetTestURL(std::wstring test_case_path,
                          const std::wstring& test_case);

  virtual void SetUp();
  virtual void TearDown();

  // Don't refactor away; some unittests override this!
  virtual void CreateEmptyWindow();

  static const char* kJavascriptDelayExitScript;

 protected:
  // Location of SOURCE_ROOT/webkit/data/
  std::wstring data_dir_;

  TestShell* test_shell_;
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_TEST_H__

