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

// NPAPI test helper classes.

#include "chrome/test/ui/npapi_test_helper.h"

NPAPITester::NPAPITester()
    : UITest() {
}

void NPAPITester::SetUp() {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  std::wstring plugins_directory = browser_directory_ + L"\\plugins";
  std::wstring plugin_src = browser_directory_ + L"\\npapi_test_plugin.dll";
  plugin_dll_ = plugins_directory + L"\\npapi_test_plugin.dll";

  CreateDirectory(plugins_directory.c_str(), NULL);
  CopyFile(plugin_src.c_str(), plugin_dll_.c_str(), FALSE);

  UITest::SetUp();
}

void NPAPITester::TearDown() {
  DeleteFile(plugin_dll_.c_str());
  UITest::TearDown();
}


// NPAPIVisiblePluginTester members.
void NPAPIVisiblePluginTester::SetUp() {
  show_window_ = true;
  NPAPITester::SetUp();
}
