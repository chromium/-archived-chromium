// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webframe.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class DomOperationsTests : public TestShellTest {
 public:
  // Test function GetAllSavableResourceLinksForCurrentPage with a web page.
  // We expect result of GetAllSavableResourceLinksForCurrentPage exactly
  // matches expected_resources_set.
  void GetSavableResourceLinksForPage(const FilePath& page_file_path,
      const std::set<GURL>& expected_resources_set);

 protected:
  // testing::Test
  virtual void SetUp() {
    TestShellTest::SetUp();
  }

  virtual void TearDown() {
    TestShellTest::TearDown();
  }
};


void DomOperationsTests::GetSavableResourceLinksForPage(
    const FilePath& page_file_path,
    const std::set<GURL>& expected_resources_set) {
  // Convert local file path to file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  // Load the test file.
  test_shell_->ResetTestController();
  std::wstring file_wurl = ASCIIToWide(file_url.spec());
  test_shell_->LoadURL(file_wurl.c_str());
  test_shell_->WaitTestFinished();
  // Get all savable resource links for the page.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrers_list;
  std::vector<GURL> frames_list;
  webkit_glue::SavableResourcesResult result(&resources_list,
                                             &referrers_list,
                                             &frames_list);

  GURL main_page_gurl(WideToUTF8(file_wurl));
  ASSERT_TRUE(webkit_glue::GetAllSavableResourceLinksForCurrentPage(
      test_shell_->webView(), main_page_gurl, &result));
  // Check all links of sub-resource
  for (std::vector<GURL>::const_iterator cit = resources_list.begin();
       cit != resources_list.end(); ++cit) {
    ASSERT_TRUE(expected_resources_set.find(*cit) !=
                expected_resources_set.end());
  }
  // Check all links of frame.
  for (std::vector<GURL>::const_iterator cit = frames_list.begin();
       cit != frames_list.end(); ++cit) {
    ASSERT_TRUE(expected_resources_set.find(*cit) !=
                expected_resources_set.end());
  }
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which has valid savable resource links.
TEST_F(DomOperationsTests, GetSavableResourceLinksWithPageHasValidLinks) {
  std::set<GURL> expected_resources_set;
  // Set directory of test data.
  FilePath page_file_path = data_dir_.AppendASCII("dom_serializer");

  const char* expected_sub_resource_links[] = {
    "file:///c:/yt/css/base_all-vfl36460.css",
    "file:///c:/yt/js/base_all_with_bidi-vfl36451.js",
    "file:///c:/yt/img/pixel-vfl73.gif"
  };
  const char* expected_frame_links[] = {
    "youtube_1.htm",
    "youtube_2.htm"
  };
  // Add all expected links of sub-resource to expected set.
  for (size_t i = 0; i < arraysize(expected_sub_resource_links); ++i)
    expected_resources_set.insert(GURL(expected_sub_resource_links[i]));
  // Add all expected links of frame to expected set.
  for (size_t i = 0; i < arraysize(expected_frame_links); ++i) {
    const FilePath expected_frame_url =
        page_file_path.AppendASCII(expected_frame_links[i]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_frame_url));
  }

  page_file_path = page_file_path.AppendASCII("youtube_1.htm");
  GetSavableResourceLinksForPage(page_file_path, expected_resources_set);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which does not have valid savable resource links.
TEST_F(DomOperationsTests, GetSavableResourceLinksWithPageHasInvalidLinks) {
  std::set<GURL> expected_resources_set;
  // Set directory of test data.
  FilePath page_file_path = data_dir_.AppendASCII("dom_serializer");

  const char* expected_frame_links[] = {
    "youtube_2.htm"
  };
  // Add all expected links of frame to expected set.
  for (size_t i = 0; i < arraysize(expected_frame_links); ++i) {
    FilePath expected_frame_url =
        page_file_path.AppendASCII(expected_frame_links[i]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_frame_url));
  }

  page_file_path = page_file_path.AppendASCII("youtube_2.htm");
  GetSavableResourceLinksForPage(page_file_path, expected_resources_set);
}

// Tests ParseIconSizes with various input.
TEST_F(DomOperationsTests, ParseIconSizes) {
  struct TestData {
    const std::wstring input;
    const bool expected_result;
    const bool is_any;
    const size_t expected_size_count;
    const int width1;
    const int height1;
    const int width2;
    const int height2;
  } data[] = {
    // Bogus input cases.
    { L"10",         false, false, 0, 0, 0, 0, 0 },
    { L"10 10",      false, false, 0, 0, 0, 0, 0 },
    { L"010",        false, false, 0, 0, 0, 0, 0 },
    { L" 010 ",      false, false, 0, 0, 0, 0, 0 },
    { L" 10x ",      false, false, 0, 0, 0, 0, 0 },
    { L" x10 ",      false, false, 0, 0, 0, 0, 0 },
    { L"any 10x10",  false, false, 0, 0, 0, 0, 0 },
    { L"",           false, false, 0, 0, 0, 0, 0 },
    { L"10ax11",     false, false, 0, 0, 0, 0, 0 },

    // Any.
    { L"any",        true, true, 0, 0, 0, 0, 0 },
    { L" any",       true, true, 0, 0, 0, 0, 0 },
    { L" any ",      true, true, 0, 0, 0, 0, 0 },

    // Sizes.
    { L"10x11",      true, false, 1, 10, 11, 0, 0 },
    { L" 10x11 ",    true, false, 1, 10, 11, 0, 0 },
    { L" 10x11 1x2", true, false, 2, 10, 11, 1, 2 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    bool is_any;
    std::vector<gfx::Size> sizes;
    const bool result =
        webkit_glue::ParseIconSizes(data[i].input, &sizes, &is_any);
    ASSERT_EQ(result, data[i].expected_result);
    if (result) {
      ASSERT_EQ(data[i].is_any, is_any);
      ASSERT_EQ(data[i].expected_size_count, sizes.size());
      if (sizes.size() > 0) {
        ASSERT_EQ(data[i].width1, sizes[0].width());
        ASSERT_EQ(data[i].height1, sizes[0].height());
      }
      if (sizes.size() > 1) {
        ASSERT_EQ(data[i].width2, sizes[1].width());
        ASSERT_EQ(data[i].height2, sizes[1].height());
      }
    }
  }
}

}  // namespace
