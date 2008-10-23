// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/clipboard.h"
#include "base/platform_test.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef PlatformTest ClipboardTest;

TEST_F(ClipboardTest, ClearTest) {
  Clipboard clipboard;

  clipboard.Clear();
  clipboard.WriteText(L"erase me");
  clipboard.Clear();
  EXPECT_EQ(false, clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextFormatType()));
  EXPECT_EQ(false, clipboard.IsFormatAvailable(
      Clipboard::GetHtmlFormatType()));
}

TEST_F(ClipboardTest, TextTest) {
  Clipboard clipboard;

  std::wstring text(L"This is a wstring!#$"), text_result;
  std::string ascii_text;

  clipboard.Clear();
  clipboard.WriteText(text);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType()));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextFormatType()));
  clipboard.ReadText(&text_result);
  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(&ascii_text);
  EXPECT_EQ(WideToUTF8(text), ascii_text);
}

TEST_F(ClipboardTest, OverwriteTest) {
  Clipboard clipboard;

  std::wstring text1(L"first string"), text2(L"second string"), text_result;

  clipboard.Clear();
  clipboard.WriteText(text1);
  clipboard.WriteText(text2);

  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType()));
  clipboard.ReadText(&text_result);
  EXPECT_EQ(text2, text_result);
}

TEST_F(ClipboardTest, HTMLTest) {
  Clipboard clipboard;

  std::wstring markup(L"<strong>Hi!</string>"), markup_result;
  std::string url("http://www.example.com/"), url_result;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetHtmlFormatType()));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif
}

TEST_F(ClipboardTest, TrickyHTMLTest) {
  Clipboard clipboard;

  std::wstring markup(L"<em>Bye!<!--EndFragment --></em>"), markup_result;
  std::string url, url_result;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetHtmlFormatType()));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif
}

// TODO(estade): Port the following test (decide what target we use for urls)
#if !defined(OS_LINUX)
TEST_F(ClipboardTest, BookmarkTest) {
  Clipboard clipboard;

  std::wstring title(L"The Example Company"), title_result;
  std::string url("http://www.example.com/"), url_result;

  clipboard.Clear();
  clipboard.WriteBookmark(title, url);
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(Clipboard::GetUrlWFormatType()));
  clipboard.ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
}
#endif

TEST_F(ClipboardTest, MultiFormatTest) {
  Clipboard clipboard;

  std::wstring text(L"Hi!"), text_result;
  std::wstring markup(L"<strong>Hi!</string>"), markup_result;
  std::string url("http://www.example.com/"), url_result;
  std::string ascii_text;

  clipboard.Clear();
  clipboard.WriteHTML(markup, url);
  clipboard.WriteText(text);
  EXPECT_EQ(true,
      clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType()));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType()));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextFormatType()));
  clipboard.ReadHTML(&markup_result, &url_result);
  EXPECT_EQ(markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif
  clipboard.ReadText(&text_result);
  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(&ascii_text);
  EXPECT_EQ(WideToUTF8(text), ascii_text);
}

// TODO(estade): Port the following tests (decide what targets we use for files)
#if !defined(OS_LINUX)
// Files for this test don't actually need to exist on the file system, just
// don't try to use a non-existent file you've retrieved from the clipboard.
TEST_F(ClipboardTest, FileTest) {
  Clipboard clipboard;
  clipboard.Clear();
#if defined(OS_WIN)
  std::wstring file = L"C:\\Downloads\\My Downloads\\A Special File.txt";
#else
  // OS X will print a warning message if we stick a non-existant file on the
  // clipboard.
  std::wstring file = L"/usr/bin/make";
#endif
  clipboard.WriteFile(file);
  std::wstring out_file;
  clipboard.ReadFile(&out_file);
  EXPECT_EQ(file, out_file);
}

TEST_F(ClipboardTest, MultipleFilesTest) {
  Clipboard clipboard;
  clipboard.Clear();
  
#if defined(OS_WIN)
  std::wstring file1 = L"C:\\Downloads\\My Downloads\\File 1.exe";
  std::wstring file2 = L"C:\\Downloads\\My Downloads\\File 2.pdf";
  std::wstring file3 = L"C:\\Downloads\\My Downloads\\File 3.doc";
#elif defined(OS_MACOSX)
  // OS X will print a warning message if we stick a non-existant file on the
  // clipboard.
  std::wstring file1 = L"/usr/bin/make";
  std::wstring file2 = L"/usr/bin/man";
  std::wstring file3 = L"/usr/bin/perl";
#endif
  std::vector<std::wstring> files;
  files.push_back(file1);
  files.push_back(file2);
  files.push_back(file3);
  clipboard.WriteFiles(files);

  std::vector<std::wstring> out_files;
  clipboard.ReadFiles(&out_files);

  EXPECT_EQ(files.size(), out_files.size());
  for (size_t i = 0; i < out_files.size(); ++i)
    EXPECT_EQ(files[i], out_files[i]);
}
#endif  // !defined(OS_LINUX)

#if defined(OS_WIN)  // Windows only tests.
TEST_F(ClipboardTest, HyperlinkTest) {
  Clipboard clipboard;
  
  std::wstring title(L"The Example Company"), title_result;
  std::string url("http://www.example.com/"), url_result;
  std::wstring html(L"<a href=\"http://www.example.com/\">"
                    L"The Example Company</a>"), html_result;
  
  clipboard.Clear();
  clipboard.WriteHyperlink(title, url);
  EXPECT_EQ(true,
            clipboard.IsFormatAvailable(Clipboard::GetUrlWFormatType()));
  EXPECT_EQ(true,
            clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType()));
  clipboard.ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
  clipboard.ReadHTML(&html_result, &url_result);
  EXPECT_EQ(html, html_result);
  //XXX EXPECT_FALSE(url_result.is_valid());
}

TEST_F(ClipboardTest, WebSmartPasteTest) {
  Clipboard clipboard;

  clipboard.Clear();
  clipboard.WriteWebSmartPaste();
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType()));
}

TEST_F(ClipboardTest, BitmapTest) {
  unsigned int fake_bitmap[] = {
    0x46155189, 0xF6A55C8D, 0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A, 0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F, 0x69AB32DF, 0x643A3FD1,
  };

  Clipboard clipboard;

  clipboard.Clear();
  clipboard.WriteBitmap(fake_bitmap, gfx::Size(3, 4));
  EXPECT_EQ(true, clipboard.IsFormatAvailable(
                      Clipboard::GetBitmapFormatType()));
}
#endif
