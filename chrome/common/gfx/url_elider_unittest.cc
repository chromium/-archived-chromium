// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/url_elider.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/views/label.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace gfx;

namespace {

const wchar_t kEllipsis[] = L"\x2026";

struct Testcase {
  const std::string input;
  const std::wstring output;
};

struct WideTestcase {
  const std::wstring input;
  const std::wstring output;
};

void RunTest(Testcase* testcases, size_t num_testcases) {
  static const ChromeFont font;
  for (size_t i = 0; i < num_testcases; ++i) {
    const GURL url(testcases[i].input);
    // Should we test with non-empty language list?
    // That's kinda redundant with net_util_unittests.
    EXPECT_EQ(testcases[i].output,
              ElideUrl(url, font, font.GetStringWidth(testcases[i].output),
                       std::wstring()));
  }
}

}  // namespace

// Test eliding of commonplace URLs.
TEST(URLEliderTest, TestGeneralEliding) {
  const std::wstring kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/intl/en/ads/",
     L"http://www.google.com/intl/en/ads/"},
    {"http://www.google.com/intl/en/ads/", L"www.google.com/intl/en/ads/"},
    {"http://www.google.com/intl/en/ads/",
     L"google.com/intl/" + kEllipsisStr + L"/ads/"},
    {"http://www.google.com/intl/en/ads/",
     L"google.com/" + kEllipsisStr + L"/ads/"},
    {"http://www.google.com/intl/en/ads/", L"google.com/" + kEllipsisStr},
    {"http://www.google.com/intl/en/ads/", L"goog" + kEllipsisStr},
    {"https://subdomain.foo.com/bar/filename.html",
     L"subdomain.foo.com/bar/filename.html"},
    {"https://subdomain.foo.com/bar/filename.html",
     L"subdomain.foo.com/" + kEllipsisStr + L"/filename.html"},
    {"http://subdomain.foo.com/bar/filename.html",
     kEllipsisStr + L"foo.com/" + kEllipsisStr + L"/filename.html"},
    {"http://www.google.com/intl/en/ads/?aLongQueryWhichIsNotRequired",
     L"http://www.google.com/intl/en/ads/?aLongQ" + kEllipsisStr},
  };

  RunTest(testcases, arraysize(testcases));
}

// Test eliding of empty strings, URLs with ports, passwords, queries, etc.
TEST(URLEliderTest, TestMoreEliding) {
  const std::wstring kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/foo?bar", L"http://www.google.com/foo?bar"},
    {"http://xyz.google.com/foo?bar", L"xyz.google.com/foo?" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", L"xyz.google.com/foo" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", L"xyz.google.com/fo" + kEllipsisStr},
    {"http://a.b.com/pathname/c?d", L"a.b.com/" + kEllipsisStr + L"/c?d"},
    {"", L""},
    {"http://foo.bar..example.com...hello/test/filename.html",
     L"foo.bar..example.com...hello/" + kEllipsisStr + L"/filename.html"},
    {"http://foo.bar../", L"http://foo.bar../"},
    {"http://xn--1lq90i.cn/foo", L"http://\x5317\x4eac.cn/foo"},
    {"http://me:mypass@secrethost.com:99/foo?bar#baz",
     L"http://secrethost.com:99/foo?bar#baz"},
    {"http://me:mypass@ss%xxfdsf.com/foo", L"http://ss%25xxfdsf.com/foo"},
    {"mailto:elgoato@elgoato.com", L"mailto:elgoato@elgoato.com"},
    {"javascript:click(0)", L"javascript:click(0)"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     L"chess.eecs.berkeley.edu:4430/login/arbitfilename"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     kEllipsisStr + L"berkeley.edu:4430/" + kEllipsisStr + L"/arbitfilename"},

    // Unescaping.
    {"http://www/%E4%BD%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     L"http://www/\x4f60\x597d?q=\x4f60\x597d#\x4f60"},

    // Invalid unescaping for path. The ref will always be valid UTF-8. We don't
    // bother to do too many edge cases, since these are handled by the escaper
    // unittest.
    {"http://www/%E4%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     L"http://www/%E4%A0%E5%A5%BD?q=\x4f60\x597d#\x4f60"},
  };

  RunTest(testcases, arraysize(testcases));
}

// Test eliding of file: URLs.
TEST(URLEliderTest, TestFileURLEliding) {
  const std::wstring kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"file:///C:/path1/path2/path3/filename",
     L"file:///C:/path1/path2/path3/filename"},
    {"file:///C:/path1/path2/path3/filename",
     L"C:/path1/path2/path3/filename"},
    {"file:///C:path1/path2/path3/filename",
     L"C:/path1/path2/" + kEllipsisStr + L"/filename"},
    {"file:///C:path1/path2/path3/filename",
     L"C:/path1/" + kEllipsisStr + L"/filename"},
    {"file:///C:path1/path2/path3/filename",
     L"C:/" + kEllipsisStr + L"/filename"},
    {"file://filer/foo/bar/file", L"filer/foo/bar/file"},
    {"file://filer/foo/bar/file", L"filer/foo/" + kEllipsisStr + L"/file"},
    {"file://filer/foo/bar/file", L"filer/" + kEllipsisStr + L"/file"},
  };

  RunTest(testcases, arraysize(testcases));
}

TEST(URLEliderTest, ElideTextLongStrings) {
  const std::wstring kEllipsisStr(kEllipsis);
  std::wstring data_scheme(L"data:text/plain,");

  std::wstring ten_a(10, L'a');
  std::wstring hundred_a(100, L'a');
  std::wstring thousand_a(1000, L'a');
  std::wstring ten_thousand_a(10000, L'a');
  std::wstring hundred_thousand_a(100000, L'a');
  std::wstring million_a(1000000, L'a');

  WideTestcase testcases[] = {
     {data_scheme + ten_a,
      data_scheme + ten_a},
     {data_scheme + hundred_a,
      data_scheme + hundred_a},
     {data_scheme + thousand_a,
      data_scheme + std::wstring(156, L'a') + kEllipsisStr},
     {data_scheme + ten_thousand_a,
      data_scheme + std::wstring(156, L'a') + kEllipsisStr},
     {data_scheme + hundred_thousand_a,
      data_scheme + std::wstring(156, L'a') + kEllipsisStr},
     {data_scheme + million_a,
      data_scheme + std::wstring(156, L'a') + kEllipsisStr},
  };

  for (size_t i = 0; i < arraysize(testcases); ++i) {
    const ChromeFont font;
    EXPECT_EQ(testcases[i].output,
              ElideText(testcases[i].input, font,
                        font.GetStringWidth(testcases[i].output)));
    EXPECT_EQ(kEllipsisStr,
              ElideText(testcases[i].input, font,
                        font.GetStringWidth(kEllipsisStr)));
  }
}

