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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/save_package.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

const std::wstring kTestDir = L"save_page";

class SavePageTest : public UITest {
 protected:
  SavePageTest() : UITest() {}

  void CheckFile(const std::wstring& client_file,
                 const std::wstring& server_file,
                 bool check_equal) {
    bool exist = false;
    for (int i = 0; i < 20; ++i) {
      if (file_util::PathExists(client_file)) {
        exist = true;
        break;
      }
      Sleep(kWaitForActionMaxMsec / 20);
    }
    EXPECT_TRUE(exist);

    if (check_equal) {
      std::wstring server_file_name;
      ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                                   &server_file_name));
      server_file_name += L"\\" + kTestDir + L"\\" + server_file;
      ASSERT_TRUE(file_util::PathExists(server_file_name));

      int64 client_file_size = 0;
      int64 server_file_size = 0;
      EXPECT_TRUE(file_util::GetFileSize(client_file, &client_file_size));
      EXPECT_TRUE(file_util::GetFileSize(server_file_name, &server_file_size));
      EXPECT_EQ(client_file_size, server_file_size);
      EXPECT_PRED2(file_util::ContentsEqual, client_file, server_file_name);
    }

    EXPECT_TRUE(DieFileDie(client_file, false));
  }

  virtual void SetUp() {
    UITest::SetUp();
    EXPECT_TRUE(file_util::CreateNewTempDirectory(L"", &save_dir_));
    save_dir_ += file_util::kPathSeparator;
  }

  std::wstring save_dir_;
};

TEST_F(SavePageTest, SaveHTMLOnly) {
  std::wstring file_name = L"a.htm";
  std::wstring full_file_name = save_dir_ + file_name;
  std::wstring dir = save_dir_ + L"a_files";

  GURL url = URLRequestMockHTTPJob::GetMockUrl(kTestDir + L"/" + file_name);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  EXPECT_TRUE(tab->SavePage(full_file_name, dir,
      SavePackage::SAVE_AS_ONLY_HTML));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));

  CheckFile(full_file_name, file_name, true);
  EXPECT_FALSE(file_util::PathExists(dir));
}

TEST_F(SavePageTest, SaveCompleteHTML) {
  std::wstring file_name = L"b.htm";
  std::wstring full_file_name = save_dir_ + file_name;
  std::wstring dir = save_dir_ + L"b_files";

  GURL url = URLRequestMockHTTPJob::GetMockUrl(kTestDir + L"/" + file_name);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  EXPECT_TRUE(tab->SavePage(full_file_name, dir,
      SavePackage::SAVE_AS_COMPLETE_HTML));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));

  CheckFile(dir + L"\\1.png", L"1.png", true);
  CheckFile(dir + L"\\1.css", L"1.css", true);
  CheckFile(full_file_name, file_name, false);
  EXPECT_TRUE(DieFileDie(dir, true));
}

TEST_F(SavePageTest, NoSave) {
  std::wstring file_name = L"c.htm";
  std::wstring full_file_name = save_dir_ + file_name;
  std::wstring dir = save_dir_ + L"c_files";

  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(GURL(L"about:blank")));
  WaitUntilTabCount(1);

  EXPECT_FALSE(tab->SavePage(full_file_name, dir,
      SavePackage::SAVE_AS_ONLY_HTML));
  EXPECT_FALSE(WaitForDownloadShelfVisible(tab.get()));
}
