// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
