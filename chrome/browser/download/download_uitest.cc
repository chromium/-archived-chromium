// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlwapi.h>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/automation/url_request_slow_download_job.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "net/url_request/url_request_unittest.h"

using std::wstring;

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

// Checks if the volume supports Alternate Data Streams. This is required for
// the Zone Identifier implementation.
bool VolumeSupportsADS(const std::wstring path) {
  wchar_t drive[MAX_PATH] = {0};
  wcscpy_s(drive, MAX_PATH, path.c_str());

  EXPECT_TRUE(PathStripToRootW(drive));

  DWORD fs_flags = 0;
  EXPECT_TRUE(GetVolumeInformationW(drive, NULL, 0, 0, NULL, &fs_flags, NULL,
                                    0));

  if (fs_flags & FILE_NAMED_STREAMS)
    return true;

  return false;
}

// Checks if the ZoneIdentifier is correctly set to "Internet" (3)
void CheckZoneIdentifier(const std::wstring full_path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

  std::wstring path = full_path + L":Zone.Identifier";
  HANDLE file = CreateFile(path.c_str(), GENERIC_READ, kShare, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);

  char buffer[100] = {0};
  DWORD read = 0;
  ASSERT_TRUE(ReadFile(file, buffer, 100, &read, NULL));
  CloseHandle(file);

  const char kIdentifier[] = "[ZoneTransfer]\nZoneId=3";
  ASSERT_EQ(arraysize(kIdentifier), read);

  ASSERT_EQ(0, strcmp(kIdentifier, buffer));
}

class DownloadTest : public UITest {
 protected:
  DownloadTest() : UITest() {}

  void CleanUpDownload(const std::wstring& client_filename,
                       const std::wstring& server_filename) {
    // Find the path on the client.
    std::wstring file_on_client(download_prefix_);
    file_on_client.append(client_filename);
    EXPECT_TRUE(file_util::PathExists(file_on_client));

    // Find the path on the server.
    std::wstring file_on_server;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                                 &file_on_server));
    file_on_server.append(L"\\");
    file_on_server.append(server_filename);
    ASSERT_TRUE(file_util::PathExists(file_on_server));

    // Check that we downloaded the file correctly.
    EXPECT_TRUE(file_util::ContentsEqual(file_on_server,
                                         file_on_client));

    // Check if the Zone Identifier is correclty set.
    if (VolumeSupportsADS(file_on_client))
      CheckZoneIdentifier(file_on_client);

    // Delete the client copy of the file.
    EXPECT_TRUE(file_util::Delete(file_on_client, false));
  }

  void CleanUpDownload(const std::wstring& file) {
    CleanUpDownload(file, file);
  }

  virtual void SetUp() {
    UITest::SetUp();
    download_prefix_ = GetDownloadDirectory();
    download_prefix_ += FilePath::kSeparators[0];
  }

 protected:
  void RunSizeTest(const wstring& url,
                   const wstring& expected_title_in_progress,
                   const wstring& expected_title_finished) {
    {
      EXPECT_EQ(1, GetTabCount());

      NavigateToURL(GURL(url));
      // Downloads appear in the shelf
      WaitUntilTabCount(1);
      // TODO(tc): check download status text

      // Complete sending the request.  We do this by loading a second URL in a
      // separate tab.
      scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
      EXPECT_TRUE(window->AppendTab(GURL(
          URLRequestSlowDownloadJob::kFinishDownloadUrl)));
      EXPECT_EQ(2, GetTabCount());
      // TODO(tc): check download status text

      // Make sure the download shelf is showing.
      scoped_ptr<TabProxy> dl_tab(window->GetTab(0));
      ASSERT_TRUE(dl_tab.get());
      EXPECT_TRUE(WaitForDownloadShelfVisible(dl_tab.get()));
    }

    std::wstring filename = file_util::GetFilenameFromPath(url);
    EXPECT_TRUE(file_util::PathExists(download_prefix_ + filename));

    // Delete the file we just downloaded.
    for (int i = 0; i < 10; ++i) {
      if (file_util::Delete(download_prefix_ + filename, false))
        break;
      PlatformThread::Sleep(action_max_timeout_ms() / 10);
    }
    EXPECT_FALSE(file_util::PathExists(download_prefix_ + filename));
  }

  wstring download_prefix_;
};

}  // namespace

// Download a file with non-viewable content, verify that the
// download tab opened and the file exists.
TEST_F(DownloadTest, DownloadMimeType) {
  wstring file = L"download-test1.lib";
  wstring expected_title = L"100% - " + file;

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  // No new tabs created, downloads appear in the current tab's download shelf.
  WaitUntilTabCount(1);

  // Wait until the file is downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  CleanUpDownload(file);

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
TEST_F(DownloadTest, NoDownload) {
  wstring file = L"download-test2.html";
  wstring file_path = download_prefix_;
  file_util::AppendToPath(&file_path, file);

  if (file_util::PathExists(file_path))
    ASSERT_TRUE(file_util::Delete(file_path, false));

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  WaitUntilTabCount(1);

  // Wait to see if the file will be downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  EXPECT_FALSE(file_util::PathExists(file_path));
  if (file_util::PathExists(file_path))
    ASSERT_TRUE(file_util::Delete(file_path, false));

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_FALSE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
TEST_F(DownloadTest, ContentDisposition) {
  wstring file = L"download-test3.gif";
  wstring download_file = L"download-test3-attachment.gif";
  wstring expected_title = L"100% - " + download_file;

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  WaitUntilTabCount(1);

  // Wait until the file is downloaded.
  PlatformThread::Sleep(action_timeout_ms());

  CleanUpDownload(download_file, file);

  // Ensure the download shelf is visible on the current tab.
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(tab_proxy.get()));
}

// UnknownSize and KnownSize are tests which depend on
// URLRequestSlowDownloadJob to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish.  At that time, the download will finish.
// TODO(paul): Reenable, http://code.google.com/p/chromium/issues/detail?id=7191
TEST_F(DownloadTest, DISABLED_UnknownSize) {
  std::wstring url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
  std::wstring filename = file_util::GetFilenameFromPath(url);
  RunSizeTest(url, L"32.0 KB - " + filename, L"100% - " + filename);
}

// http://b/1158253
TEST_F(DownloadTest, DISABLED_KnownSize) {
  std::wstring url(URLRequestSlowDownloadJob::kKnownSizeUrl);
  std::wstring filename = file_util::GetFilenameFromPath(url);
  RunSizeTest(url, L"71% - " + filename, L"100% - " + filename);
}

