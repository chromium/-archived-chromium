// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

const std::string kTestDir = "save_page";

const std::string kAppendedExtension = ".htm";

class SavePageTest : public UITest {
 protected:
  SavePageTest() : UITest() {}

  void CheckFile(const FilePath& client_file,
                 const FilePath& server_file,
                 bool check_equal) {
    bool exist = false;
    for (int i = 0; i < 20; ++i) {
      if (file_util::PathExists(client_file)) {
        exist = true;
        break;
      }
      PlatformThread::Sleep(sleep_timeout_ms());
    }
    EXPECT_TRUE(exist);

    if (check_equal) {
      FilePath server_file_name;
      ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                                   &server_file_name));
      server_file_name = server_file_name.AppendASCII(kTestDir)
                                         .Append(server_file);
      ASSERT_TRUE(file_util::PathExists(server_file_name));

      int64 client_file_size = 0;
      int64 server_file_size = 0;
      EXPECT_TRUE(file_util::GetFileSize(client_file, &client_file_size));
      EXPECT_TRUE(file_util::GetFileSize(server_file_name, &server_file_size));
      EXPECT_EQ(client_file_size, server_file_size);
      EXPECT_TRUE(file_util::ContentsEqual(client_file, server_file_name));
    }

    EXPECT_TRUE(DieFileDie(client_file, false));
  }

  virtual void SetUp() {
    UITest::SetUp();
    EXPECT_TRUE(file_util::CreateNewTempDirectory(FILE_PATH_LITERAL(""),
                                                  &save_dir_));

    download_dir_ = FilePath::FromWStringHack(GetDownloadDirectory());
  }

  virtual void TearDown() {
    UITest::TearDown();
    DieFileDie(save_dir_, true);
  }

  FilePath save_dir_;
  FilePath download_dir_;
};

TEST_F(SavePageTest, SaveHTMLOnly) {
  std::string file_name("a.htm");
  FilePath full_file_name = save_dir_.AppendASCII(file_name);
  FilePath dir = save_dir_.AppendASCII("a_files");

  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      UTF8ToWide(kTestDir + "/" + file_name));
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  EXPECT_TRUE(tab->SavePage(full_file_name.ToWStringHack(), dir.ToWStringHack(),
                            SavePackage::SAVE_AS_ONLY_HTML));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));

  CheckFile(full_file_name, FilePath::FromWStringHack(UTF8ToWide(file_name)),
            true);
  EXPECT_FALSE(file_util::PathExists(dir));
}

TEST_F(SavePageTest, SaveCompleteHTML) {
  std::string file_name = "b.htm";
  FilePath full_file_name = save_dir_.AppendASCII(file_name);
  FilePath dir = save_dir_.AppendASCII("b_files");

  GURL url = URLRequestMockHTTPJob::GetMockUrl(UTF8ToWide(kTestDir + "/" +
                                                          file_name));
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  EXPECT_TRUE(tab->SavePage(full_file_name.ToWStringHack(), dir.ToWStringHack(),
                            SavePackage::SAVE_AS_COMPLETE_HTML));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));

  CheckFile(dir.AppendASCII("1.png"), FilePath(FILE_PATH_LITERAL("1.png")),
                            true);
  CheckFile(dir.AppendASCII("1.css"), FilePath(FILE_PATH_LITERAL("1.css")),
                            true);
  CheckFile(full_file_name, FilePath::FromWStringHack(UTF8ToWide(file_name)),
            false);
  EXPECT_TRUE(DieFileDie(dir, true));
}

TEST_F(SavePageTest, NoSave) {
  std::string file_name = "c.htm";
  FilePath full_file_name = save_dir_.AppendASCII(file_name);
  FilePath dir = save_dir_.AppendASCII("c_files");

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(GURL("about:blank")));
  WaitUntilTabCount(1);

  EXPECT_FALSE(tab->SavePage(full_file_name.ToWStringHack(),
                             dir.ToWStringHack(),
                             SavePackage::SAVE_AS_ONLY_HTML));
  EXPECT_FALSE(WaitForDownloadShelfVisible(tab.get()));
}

TEST_F(SavePageTest, FilenameFromPageTitle) {
  std::string file_name = "b.htm";

  FilePath full_file_name = download_dir_.AppendASCII(
      "Test page for saving page feature" + kAppendedExtension);
  FilePath dir = download_dir_.AppendASCII(
      "Test page for saving page feature_files");

  GURL url = URLRequestMockHTTPJob::GetMockUrl(UTF8ToWide(kTestDir + "/" +
                                               file_name));
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  automation()->SavePackageShouldPromptUser(false);
  EXPECT_TRUE(browser->RunCommandAsync(IDC_SAVE_PAGE));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));
  automation()->SavePackageShouldPromptUser(true);

  CheckFile(dir.AppendASCII("1.png"), FilePath(FILE_PATH_LITERAL("1.png")),
                            true);
  CheckFile(dir.AppendASCII("1.css"), FilePath(FILE_PATH_LITERAL("1.css")),
                            true);
  CheckFile(full_file_name, FilePath::FromWStringHack(UTF8ToWide(file_name)),
            false);
  EXPECT_TRUE(DieFileDie(full_file_name, false));
  EXPECT_TRUE(DieFileDie(dir, true));
}

// This tests that a webpage with the title "test.exe" is saved as
// "test.exe.htm".
// We probably don't care to handle this on Linux or Mac.
#if defined(OS_WIN)
TEST_F(SavePageTest, CleanFilenameFromPageTitle) {
  std::string file_name = "c.htm";
  FilePath full_file_name = download_dir_.AppendASCII("test.exe" +
                                                      kAppendedExtension);
  FilePath dir = download_dir_.AppendASCII("test.exe_files");

  GURL url = URLRequestMockHTTPJob::GetMockUrl(UTF8ToWide(kTestDir + "/" +
                                               file_name));
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  automation()->SavePackageShouldPromptUser(false);
  EXPECT_TRUE(browser->RunCommandAsync(IDC_SAVE_PAGE));
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab.get()));
  automation()->SavePackageShouldPromptUser(true);

  CheckFile(full_file_name, FilePath::FromWStringHack(UTF8ToWide(file_name)),
            false);
  EXPECT_TRUE(DieFileDie(full_file_name, false));
  EXPECT_TRUE(DieFileDie(dir, true));
}
#endif
