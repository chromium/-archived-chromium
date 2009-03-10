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

#include "chrome/test/ui/ui_test.h"

#include "base/file_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

class LayoutPluginTester : public UITest {
};

}  // namespace

// Make sure that navigating away from a plugin referenced by JS doesn't
// crash.
TEST_F(LayoutPluginTester, UnloadNoCrash) {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  std::wstring plugins_directory = browser_directory_;
  plugins_directory += L"\\plugins";
  std::wstring plugin_src = browser_directory_;
  plugin_src += L"\\npapi_layout_test_plugin.dll";
  std::wstring plugin_dest = plugins_directory;
  plugin_dest += L"\\npapi_layout_test_plugin.dll";

  CreateDirectory(plugins_directory.c_str(), NULL);
  CopyFile(plugin_src.c_str(), plugin_dest.c_str(), true /* overwrite */);

  std::wstring path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  file_util::AppendToPath(&path, L"npapi/layout_test_plugin.html");
  NavigateToURL(net::FilePathToFileURL(path));

  std::wstring title;
  TabProxy* tab = GetActiveTab();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(L"Layout Test Plugin Test", title);

  ASSERT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(L"", title);

  delete tab;
}
