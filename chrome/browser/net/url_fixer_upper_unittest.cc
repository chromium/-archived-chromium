// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class URLFixerUpperTest : public testing::Test {
  };
};

std::ostream& operator<<(std::ostream& os, const url_parse::Component& part) {
  return os << "(begin=" << part.begin << ", len=" << part.len << ")";
}

struct segment_case {
  const std::wstring input;
  const std::wstring result;
  const url_parse::Component scheme;
  const url_parse::Component username;
  const url_parse::Component password;
  const url_parse::Component host;
  const url_parse::Component port;
  const url_parse::Component path;
  const url_parse::Component query;
  const url_parse::Component ref;
};

static const segment_case segment_cases[] = {
  { L"http://www.google.com/", L"http",
    url_parse::Component(0, 4), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(7, 14), // host
    url_parse::Component(), // port
    url_parse::Component(21, 1), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { L"aBoUt:vErSiOn", L"about",
    url_parse::Component(0, 5), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { L"    www.google.com:124?foo#", L"http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(4, 14), // host
    url_parse::Component(19, 3), // port
    url_parse::Component(), // path
    url_parse::Component(23, 3), // query
    url_parse::Component(27, 0), // ref
  },
  { L"user@www.google.com", L"http",
    url_parse::Component(), // scheme
    url_parse::Component(0, 4), // username
    url_parse::Component(), // password
    url_parse::Component(5, 14), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { L"ftp:/user:P:a$$Wd@..ftp.google.com...::23///pub?foo#bar", L"ftp",
    url_parse::Component(0, 3), // scheme
    url_parse::Component(5, 4), // username
    url_parse::Component(10, 7), // password
    url_parse::Component(18, 20), // host
    url_parse::Component(39, 2), // port
    url_parse::Component(41, 6), // path
    url_parse::Component(48, 3), // query
    url_parse::Component(52, 3), // ref
  },
};

TEST(URLFixerUpperTest, SegmentURL) {
  std::wstring result;
  url_parse::Parsed parts;

  for (int i = 0; i < arraysize(segment_cases); ++i) {
    segment_case value = segment_cases[i];
    result = URLFixerUpper::SegmentURL(value.input, &parts);
    EXPECT_EQ(value.result, result);
    EXPECT_EQ(value.scheme, parts.scheme);
    EXPECT_EQ(value.username, parts.username);
    EXPECT_EQ(value.password, parts.password);
    EXPECT_EQ(value.host, parts.host);
    EXPECT_EQ(value.port, parts.port);
    EXPECT_EQ(value.path, parts.path);
    EXPECT_EQ(value.query, parts.query);
    EXPECT_EQ(value.ref, parts.ref);
  }
}

// Creates a file and returns its full name as well as the decomposed
// version. Example:
//    full_path = "c:\foo\bar.txt"
//    dir = "c:\foo"
//    file_name = "bar.txt"
static bool MakeTempFile(const std::wstring& dir,
                         const std::wstring& file_name,
                         std::wstring* full_path) {
  *full_path = dir + L"\\" + file_name;

  HANDLE hfile = CreateFile(full_path->c_str(), GENERIC_READ | GENERIC_WRITE,
                            0, NULL, CREATE_ALWAYS, 0, NULL);
  if (hfile == NULL || hfile == INVALID_HANDLE_VALUE)
    return false;
  CloseHandle(hfile);
  return true;
}

// Returns true if the given URL is a file: URL that matches the given file
static bool IsMatchingFileURL(const std::wstring& url,
                              const std::wstring& full_file_path) {
  if (url.length() <= 8)
    return false;
  if (std::wstring(L"file:///") != url.substr(0, 8))
    return false; // no file:/// prefix
  if (url.find('\\') != std::wstring::npos)
    return false; // contains backslashes

  std::wstring derived_path;
  net::FileURLToFilePath(GURL(url), &derived_path);
  return (derived_path.length() == full_file_path.length()) &&
      std::equal(derived_path.begin(), derived_path.end(),
                 full_file_path.begin(), CaseInsensitiveCompare<wchar_t>());
}

struct fixup_case {
  const std::wstring input;
  const std::wstring desired_tld;
  const std::wstring output;
} fixup_cases[] = {
  {L"www.google.com", L"", L"http://www.google.com/"},
  {L" www.google.com     ", L"", L"http://www.google.com/"},
  {L" foo.com/asdf  bar", L"", L"http://foo.com/asdf  bar"},
  {L"..www.google.com..", L"", L"http://www.google.com./"},
  {L"http://......", L"", L"http://....../"},
  {L"http://host.com:ninety-two/", L"", L"http://host.com/"},
  {L"http://host.com:ninety-two?foo", L"", L"http://host.com/?foo"},
  {L"google.com:123", L"", L"http://google.com:123/"},
  {L"about:", L"", L"about:"},
  {L"about:version", L"", L"about:version"},
  {L"www:123", L"", L"http://www:123/"},
  {L"   www:123", L"", L"http://www:123/"},
  {L"www.google.com?foo", L"", L"http://www.google.com/?foo"},
  {L"www.google.com#foo", L"", L"http://www.google.com/#foo"},
  {L"www.google.com?", L"", L"http://www.google.com/?"},
  {L"www.google.com#", L"", L"http://www.google.com/#"},
  {L"www.google.com:123?foo#bar", L"", L"http://www.google.com:123/?foo#bar"},
  {L"user@www.google.com", L"", L"http://user@www.google.com/"},
  {L"\x6C34.com", L"", L"http://\x6C34.com/" },
  // It would be better if this next case got treated as http, but I don't see
  // a clean way to guess this isn't the new-and-exciting "user" scheme.
  {L"user:passwd@www.google.com:8080/", L"", L"user:passwd@www.google.com:8080/"},
  //{L"file:///c:/foo/bar%20baz.txt", L"", L"file:///C:/foo/bar%20baz.txt"},
  {L"ftp.google.com", L"", L"ftp://ftp.google.com/"},
  {L"    ftp.google.com", L"", L"ftp://ftp.google.com/"},
  {L"FTP.GooGle.com", L"", L"ftp://FTP.GooGle.com/"},
  {L"ftpblah.google.com", L"", L"http://ftpblah.google.com/"},
  {L"ftp", L"", L"http://ftp/"},
  {L"google.ftp.com", L"", L"http://google.ftp.com/"},
};

TEST(URLFixerUpperTest, FixupURL) {
  std::wstring output;

  for (int i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
    output = URLFixerUpper::FixupURL(value.input, value.desired_tld);
    EXPECT_EQ(value.output, output);
  }

  // Check the TLD-appending functionality
  fixup_case tld_cases[] = {
    {L"google", L"com", L"http://www.google.com/"},
    {L"google.", L"com", L"http://www.google.com/"},
    {L"google..", L"com", L"http://www.google.com/"},
    {L".google", L"com", L"http://www.google.com/"},
    {L"www.google", L"com", L"http://www.google.com/"},
    {L"google.com", L"com", L"http://google.com/"},
    {L"http://google", L"com", L"http://www.google.com/"},
    {L"..google..", L"com", L"http://www.google.com/"},
    {L"http://www.google", L"com", L"http://www.google.com/"},
    {L"google/foo", L"com", L"http://www.google.com/foo"},
    {L"google.com/foo", L"com", L"http://google.com/foo"},
    {L"google/?foo=.com", L"com", L"http://www.google.com/?foo=.com"},
    {L"www.google/?foo=www.", L"com", L"http://www.google.com/?foo=www."},
    {L"google.com/?foo=.com", L"com", L"http://google.com/?foo=.com"},
    {L"http://www.google.com", L"com", L"http://www.google.com/"},
    {L"google:123", L"com", L"http://www.google.com:123/"},
    {L"http://google:123", L"com", L"http://www.google.com:123/"},
  };
  for (int i = 0; i < arraysize(tld_cases); ++i) {
    fixup_case value = tld_cases[i];
    output = URLFixerUpper::FixupURL(value.input, value.desired_tld);
    EXPECT_EQ(value.output, output);
  }
}

// Test different types of file inputs to URIFixerUpper::FixupURL. This
// doesn't go into the nice array of fixups above since the file input
// has to exist.
TEST(URLFixerUpperTest, FixupFile) {
  // this "original" filename is the one we tweak to get all the variations
  std::wstring dir;
  std::wstring original;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(dir, L"url fixer upper existing file.txt",
                           &original));

  // reference path
  std::wstring golden =
      UTF8ToWide(net::FilePathToFileURL(original).spec());

  // c:\foo\bar.txt -> file:///c:/foo/bar.txt (basic)
  std::wstring fixedup = URLFixerUpper::FixupURL(original, L"");
  EXPECT_EQ(golden, fixedup);

  // c|/foo\bar.txt -> file:///c:/foo/bar.txt (pipe allowed instead of colon)
  std::wstring cur(original);
  EXPECT_EQ(':', cur[1]);
  cur[1] = '|';
  fixedup = URLFixerUpper::FixupURL(cur, L"");
  EXPECT_EQ(golden, fixedup);

  fixup_case file_cases[] = {
    // File URLs go through GURL, which tries to escape intelligently.
    {L"c:\\This%20is a non-existent file.txt", L"", L"file:///C:/This%2520is%20a%20non-existent%20file.txt"},

    // \\foo\bar.txt -> file://foo/bar.txt
    // UNC paths, this file won't exist, but since there are no escapes, it
    // should be returned just converted to a file: URL.
    {L"\\\\SomeNonexistentHost\\foo\\bar.txt", L"", L"file://somenonexistenthost/foo/bar.txt"},
    {L"//SomeNonexistentHost\\foo/bar.txt", L"", L"file://somenonexistenthost/foo/bar.txt"},
    {L"file:///C:/foo/bar", L"", L"file:///C:/foo/bar"},

    // These are fixups we don't do, but could consider:
    //
    //   {L"file://C:/foo/bar", L"", L"file:///C:/foo/bar"},
    //   {L"file:c:", L"", L"file:///c:/"},
    //   {L"file:c:WINDOWS", L"", L"file:///c:/WINDOWS"},
    //   {L"file:c|Program Files", L"", L"file:///c:/Program Files"},
    //   {L"file:///foo:/bar", L"", L"file://foo/bar"},
    //   {L"file:/file", L"", L"file://file/"},
    //   {L"file:////////c:\\foo", L"", L"file:///c:/foo"},
    //   {L"file://server/folder/file", L"", L"file://server/folder/file"},
    //   {L"file:/\\/server\\folder/file", L"", L"file://server/folder/file"},
  };
  for (int i = 0; i < arraysize(file_cases); i++) {
    fixedup = URLFixerUpper::FixupURL(file_cases[i].input,
                                      file_cases[i].desired_tld);
    EXPECT_EQ(file_cases[i].output, fixedup);
  }

  EXPECT_TRUE(DeleteFile(original.c_str()));
}

TEST(URLFixerUpperTest, FixupRelativeFile) {
  std::wstring full_path, dir;
  std::wstring file_part(L"url_fixer_upper_existing_file.txt");
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(dir, file_part, &full_path));

  // make sure we pass through good URLs
  std::wstring fixedup;
    for (int i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
    fixedup = URLFixerUpper::FixupRelativeFile(dir, value.input);
    EXPECT_EQ(value.output, fixedup);
  }

  // make sure the existing file got fixed-up to a file URL, and that there
  // are no backslashes
  fixedup = URLFixerUpper::FixupRelativeFile(dir, file_part);
  EXPECT_PRED2(IsMatchingFileURL, fixedup, full_path);
  EXPECT_TRUE(DeleteFile(full_path.c_str()));

  // create a filename we know doesn't exist and make sure it doesn't get
  // fixed up to a file URL
  std::wstring nonexistent_file(L"url_fixer_upper_nonexistent_file.txt");
  fixedup = URLFixerUpper::FixupRelativeFile(dir, nonexistent_file);
  EXPECT_NE(std::wstring(L"file:///"), fixedup.substr(0, 8));
  EXPECT_FALSE(IsMatchingFileURL(fixedup, nonexistent_file));

  // make a subdir to make sure relative paths with directories work, also
  // test spaces: "app_dir\url fixer-upper dir\url fixer-upper existing file.txt"
  std::wstring sub_dir(L"url fixer-upper dir");
  std::wstring sub_file(L"url fixer-upper existing file.txt");
  std::wstring new_dir = dir + L"\\" + sub_dir;
  CreateDirectory(new_dir.c_str(), NULL);
  ASSERT_TRUE(MakeTempFile(new_dir, sub_file, &full_path));

  // test file in the subdir
  std::wstring relative_file = sub_dir + L"\\" + sub_file;
  fixedup = URLFixerUpper::FixupRelativeFile(dir, relative_file);
  EXPECT_PRED2(IsMatchingFileURL, fixedup, full_path);

  // test file in the subdir with different slashes and escaping
  relative_file = sub_dir + L"/" + sub_file;
  ReplaceSubstringsAfterOffset(&relative_file, 0, L" ", L"%20");
  fixedup = URLFixerUpper::FixupRelativeFile(dir, relative_file);
  EXPECT_PRED2(IsMatchingFileURL, fixedup, full_path);

  // test relative directories and duplicate slashes
  // (should resolve to the same file as above)
  relative_file = sub_dir + L"\\../" + sub_dir  + L"\\\\\\.\\" + sub_file;
  fixedup = URLFixerUpper::FixupRelativeFile(dir, relative_file);
  EXPECT_PRED2(IsMatchingFileURL, fixedup, full_path);

  // done with the subdir
  EXPECT_TRUE(DeleteFile(full_path.c_str()));
  EXPECT_TRUE(RemoveDirectory(new_dir.c_str()));
}

