// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/clipboard.h"
#include "base/clipboard_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class ClipboardTest : public testing::Test {
  };
}

TEST(ClipboardTest, ClearTest) {
  Clipboard clipboard;

  clipboard.Clear();
  EXPECT_EQ(false, clipboard.IsFormatAvailable(CF_TEXT));
  EXPECT_EQ(false, clipboard.IsFormatAvailable(
      ClipboardUtil::GetHtmlFormat()->cfFormat));
}

TEST(ClipboardTest, TextTest) {
  Clipboard clipboard;

  std::wstring text(L"This is a wstring!#$"), text_result;
  std::string ascii_text;

  clipboard.Clear();
  clipboard.WriteText(text);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(CF_UNICODETEXT));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(CF_TEXT));
  clipboard.ReadText(&text_result);
  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(&ascii_text);
  EXPECT_EQ(WideToUTF8(text), ascii_text);
}

TEST(ClipboardTest, HTMLTest) {
  Clipboard clipboard;

  std::wstring markup(L"<strong>Hi!</string>"), markup_result;
  std::string url("http://www.example.com/"), url_result;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      ClipboardUtil::GetHtmlFormat()->cfFormat));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
  EXPECT_EQ(url, url_result);
}

TEST(ClipboardTest, TrickyHTMLTest) {
  Clipboard clipboard;

  std::wstring markup(L"<em>Bye!<!--EndFragment --></em>"), markup_result;
  std::string url, url_result;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      ClipboardUtil::GetHtmlFormat()->cfFormat));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
  EXPECT_EQ(url, url_result);
}

TEST(ClipboardTest, BookmarkTest) {
  Clipboard clipboard;

  std::wstring title(L"The Example Company"), title_result;
  std::string url("http://www.example.com/"), url_result;

  clipboard.Clear();
  clipboard.WriteBookmark(title, url);
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(ClipboardUtil::GetUrlWFormat()->cfFormat));
  clipboard.ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
}

TEST(ClipboardTest, HyperlinkTest) {
  Clipboard clipboard;

  std::wstring title(L"The Example Company"), title_result;
  std::string url("http://www.example.com/"), url_result;
  std::wstring html(L"<a href=\"http://www.example.com/\">"
                    L"The Example Company</a>"), html_result;

  clipboard.Clear();
  clipboard.WriteHyperlink(title, url);
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(ClipboardUtil::GetUrlWFormat()->cfFormat));
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(ClipboardUtil::GetHtmlFormat()->cfFormat));
  clipboard.ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
  clipboard.ReadHTML(&html_result, &url_result);
  EXPECT_EQ(html, html_result);
  //XXX EXPECT_FALSE(url_result.is_valid());
}

TEST(ClipboardTest, MultiFormatTest) {
  Clipboard clipboard;

  std::wstring text(L"Hi!"), text_result;
  std::wstring markup(L"<strong>Hi!</string>"), markup_result;
  std::string url("http://www.example.com/"), url_result;
  std::string ascii_text;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  clipboard.WriteText(text);
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(ClipboardUtil::GetHtmlFormat()->cfFormat));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(CF_UNICODETEXT));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(CF_TEXT));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
  EXPECT_EQ(url, url_result);
  clipboard.ReadText(&text_result);
  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(&ascii_text);
  EXPECT_EQ(WideToUTF8(text), ascii_text);
}

TEST(ClipboardTest, WebSmartPasteTest) {
  Clipboard clipboard;

  clipboard.Clear();
  clipboard.WriteWebSmartPaste();
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      ClipboardUtil::GetWebKitSmartPasteFormat()->cfFormat));
}

TEST(ClipboardTest, BitmapTest) {
  unsigned int fake_bitmap[] = {
    0x46155189, 0xF6A55C8D, 0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A, 0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F, 0x69AB32DF, 0x643A3FD1,
  };

  Clipboard clipboard;

  clipboard.Clear();
  clipboard.WriteBitmap(fake_bitmap, gfx::Size(3, 4));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(CF_BITMAP));
}

// Files for this test don't actually need to exist on the file system, just
// don't try to use a non-existent file you've retrieved from the clipboard.
TEST(ClipboardTest, FileTest) {
  Clipboard clipboard;
  clipboard.Clear();

  std::wstring file = L"C:\\Downloads\\My Downloads\\A Special File.txt";
  clipboard.WriteFile(file);
  std::wstring out_file;
  clipboard.ReadFile(&out_file);
  EXPECT_EQ(file, out_file);
}

TEST(ClipboardTest, MultipleFilesTest) {
  Clipboard clipboard;
  clipboard.Clear();

  std::vector<std::wstring> files;
  files.push_back(L"C:\\Downloads\\My Downloads\\File 1.exe");
  files.push_back(L"C:\\Downloads\\My Downloads\\File 2.pdf");
  files.push_back(L"C:\\Downloads\\My Downloads\\File 3.doc");
  clipboard.WriteFiles(files);

  std::vector<std::wstring> out_files;
  clipboard.ReadFiles(&out_files);

  EXPECT_EQ(files.size(), out_files.size());
  for (size_t i = 0; i < out_files.size(); ++i)
    EXPECT_EQ(files[i], out_files[i]);
}

