// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/basictypes.h"
#include "base/file_util.h"
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
  const std::string input;
  const std::string result;
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
  { "http://www.google.com/", "http",
    url_parse::Component(0, 4), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(7, 14), // host
    url_parse::Component(), // port
    url_parse::Component(21, 1), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "aBoUt:vErSiOn", "about",
    url_parse::Component(0, 5), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "    www.google.com:124?foo#", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(4, 14), // host
    url_parse::Component(19, 3), // port
    url_parse::Component(), // path
    url_parse::Component(23, 3), // query
    url_parse::Component(27, 0), // ref
  },
  { "user@www.google.com", "http",
    url_parse::Component(), // scheme
    url_parse::Component(0, 4), // username
    url_parse::Component(), // password
    url_parse::Component(5, 14), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "ftp:/user:P:a$$Wd@..ftp.google.com...::23///pub?foo#bar", "ftp",
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
  std::string result;
  url_parse::Parsed parts;

  for (size_t i = 0; i < arraysize(segment_cases); ++i) {
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
static bool MakeTempFile(const FilePath& dir,
                         const FilePath& file_name,
                         FilePath* full_path) {
  *full_path = dir.Append(file_name);
  return file_util::WriteFile(full_path->ToWStringHack(), NULL, 0) == 0;
}

// Returns true if the given URL is a file: URL that matches the given file
static bool IsMatchingFileURL(const std::string& url,
                              const FilePath& full_file_path) {
  if (url.length() <= 8)
    return false;
  if (std::string("file:///") != url.substr(0, 8))
    return false; // no file:/// prefix
  if (url.find('\\') != std::string::npos)
    return false; // contains backslashes

  FilePath derived_path;
  net::FileURLToFilePath(GURL(url), &derived_path);

  FilePath::StringType derived_path_str = derived_path.value();
  return (derived_path_str.length() == full_file_path.value().length()) &&
      std::equal(derived_path_str.begin(),
                 derived_path_str.end(),
                 full_file_path.value().begin(),
                 CaseInsensitiveCompare<FilePath::CharType>());
}

struct fixup_case {
  const std::string input;
  const std::string desired_tld;
  const std::string output;
} fixup_cases[] = {
  {"www.google.com", "", "http://www.google.com/"},
  {" www.google.com     ", "", "http://www.google.com/"},
  {" foo.com/asdf  bar", "", "http://foo.com/asdf  bar"},
  {"..www.google.com..", "", "http://www.google.com./"},
  {"http://......", "", "http://....../"},
  {"http://host.com:ninety-two/", "", "http://host.com/"},
  {"http://host.com:ninety-two?foo", "", "http://host.com/?foo"},
  {"google.com:123", "", "http://google.com:123/"},
  {"about:", "", "about:"},
  {"about:version", "", "about:version"},
  {"www:123", "", "http://www:123/"},
  {"   www:123", "", "http://www:123/"},
  {"www.google.com?foo", "", "http://www.google.com/?foo"},
  {"www.google.com#foo", "", "http://www.google.com/#foo"},
  {"www.google.com?", "", "http://www.google.com/?"},
  {"www.google.com#", "", "http://www.google.com/#"},
  {"www.google.com:123?foo#bar", "", "http://www.google.com:123/?foo#bar"},
  {"user@www.google.com", "", "http://user@www.google.com/"},
  {"\xE6\xB0\xB4.com" , "", "http://\xE6\xB0\xB4.com/"},
  // It would be better if this next case got treated as http, but I don't see
  // a clean way to guess this isn't the new-and-exciting "user" scheme.
  {"user:passwd@www.google.com:8080/", "", "user:passwd@www.google.com:8080/"},
  //{"file:///c:/foo/bar%20baz.txt", "", "file:///C:/foo/bar%20baz.txt"},
  {"ftp.google.com", "", "ftp://ftp.google.com/"},
  {"    ftp.google.com", "", "ftp://ftp.google.com/"},
  {"FTP.GooGle.com", "", "ftp://FTP.GooGle.com/"},
  {"ftpblah.google.com", "", "http://ftpblah.google.com/"},
  {"ftp", "", "http://ftp/"},
  {"google.ftp.com", "", "http://google.ftp.com/"},
  // URLs which end with 0x85 (NEL in ISO-8859).
  { "http://google.com/search?q=\xd0\x85", "",
    "http://google.com/search?q=\xd0\x85"
  },
  { "http://google.com/search?q=\xec\x97\x85", "",
    "http://google.com/search?q=\xec\x97\x85"
  },
  { "http://google.com/search?q=\xf0\x90\x80\x85", "",
    "http://google.com/search?q=\xf0\x90\x80\x85"
  },
  // URLs which end with 0xA0 (non-break space in ISO-8859).
  { "http://google.com/search?q=\xd0\xa0", "",
    "http://google.com/search?q=\xd0\xa0"
  },
  { "http://google.com/search?q=\xec\x97\xa0", "",
    "http://google.com/search?q=\xec\x97\xa0"
  },
  { "http://google.com/search?q=\xf0\x90\x80\xa0", "",
    "http://google.com/search?q=\xf0\x90\x80\xa0"
  },
};

TEST(URLFixerUpperTest, FixupURL) {
  std::string output;

  for (size_t i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
    output = URLFixerUpper::FixupURL(value.input, value.desired_tld);
    EXPECT_EQ(value.output, output);
  }

  // Check the TLD-appending functionality
  fixup_case tld_cases[] = {
    {"google", "com", "http://www.google.com/"},
    {"google.", "com", "http://www.google.com/"},
    {"google..", "com", "http://www.google.com/"},
    {".google", "com", "http://www.google.com/"},
    {"www.google", "com", "http://www.google.com/"},
    {"google.com", "com", "http://google.com/"},
    {"http://google", "com", "http://www.google.com/"},
    {"..google..", "com", "http://www.google.com/"},
    {"http://www.google", "com", "http://www.google.com/"},
    {"google/foo", "com", "http://www.google.com/foo"},
    {"google.com/foo", "com", "http://google.com/foo"},
    {"google/?foo=.com", "com", "http://www.google.com/?foo=.com"},
    {"www.google/?foo=www.", "com", "http://www.google.com/?foo=www."},
    {"google.com/?foo=.com", "com", "http://google.com/?foo=.com"},
    {"http://www.google.com", "com", "http://www.google.com/"},
    {"google:123", "com", "http://www.google.com:123/"},
    {"http://google:123", "com", "http://www.google.com:123/"},
  };
  for (size_t i = 0; i < arraysize(tld_cases); ++i) {
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
  FilePath dir;
  FilePath original;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(
      dir,
      FilePath(FILE_PATH_LITERAL("url fixer upper existing file.txt")),
      &original));

  // reference path
  std::string golden = net::FilePathToFileURL(original).spec();

  // c:\foo\bar.txt -> file:///c:/foo/bar.txt (basic)
#if defined(OS_WIN)
  std::string fixedup = URLFixerUpper::FixupURL(WideToUTF8(original.value()),
                                                "");
#elif defined(OS_POSIX)
  std::string fixedup = URLFixerUpper::FixupURL(original.value(), "");
#endif
  EXPECT_EQ(golden, fixedup);

  // TODO(port): Make some equivalent tests for posix.
#if defined(OS_WIN)
  // c|/foo\bar.txt -> file:///c:/foo/bar.txt (pipe allowed instead of colon)
  std::string cur(WideToUTF8(original.value()));
  EXPECT_EQ(':', cur[1]);
  cur[1] = '|';
  fixedup = URLFixerUpper::FixupURL(cur, "");
  EXPECT_EQ(golden, fixedup);

  fixup_case file_cases[] = {
    // File URLs go through GURL, which tries to escape intelligently.
    {"c:\\This%20is a non-existent file.txt", "",
     "file:///C:/This%2520is%20a%20non-existent%20file.txt"},

    // \\foo\bar.txt -> file://foo/bar.txt
    // UNC paths, this file won't exist, but since there are no escapes, it
    // should be returned just converted to a file: URL.
    {"\\\\SomeNonexistentHost\\foo\\bar.txt", "",
     "file://somenonexistenthost/foo/bar.txt"},
    {"//SomeNonexistentHost\\foo/bar.txt", "",
     "file://somenonexistenthost/foo/bar.txt"},
    {"file:///C:/foo/bar", "", "file:///C:/foo/bar"},

    // These are fixups we don't do, but could consider:
    //
    //   {"file://C:/foo/bar", "", "file:///C:/foo/bar"},
    //   {"file:c:", "", "file:///c:/"},
    //   {"file:c:WINDOWS", "", "file:///c:/WINDOWS"},
    //   {"file:c|Program Files", "", "file:///c:/Program Files"},
    //   {"file:///foo:/bar", "", "file://foo/bar"},
    //   {"file:/file", "", "file://file/"},
    //   {"file:////////c:\\foo", "", "file:///c:/foo"},
    //   {"file://server/folder/file", "", "file://server/folder/file"},
    //   {"file:/\\/server\\folder/file", "", "file://server/folder/file"},
  };
  for (size_t i = 0; i < arraysize(file_cases); i++) {
    fixedup = URLFixerUpper::FixupURL(file_cases[i].input,
                                      file_cases[i].desired_tld);
    EXPECT_EQ(file_cases[i].output, fixedup);
  }
#endif

  EXPECT_TRUE(file_util::Delete(original, false));
}

TEST(URLFixerUpperTest, FixupRelativeFile) {
  FilePath full_path, dir;
  FilePath file_part(FILE_PATH_LITERAL("url_fixer_upper_existing_file.txt"));
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(dir, file_part, &full_path));

  // make sure we pass through good URLs
  std::string fixedup;
  for (size_t i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
#if defined(OS_WIN)
    FilePath input(UTF8ToWide(value.input));
#elif defined(OS_POSIX)
    FilePath input(value.input);
#endif
    fixedup = URLFixerUpper::FixupRelativeFile(dir, input);
    EXPECT_EQ(value.output, fixedup);
  }

  // make sure the existing file got fixed-up to a file URL, and that there
  // are no backslashes
  fixedup = URLFixerUpper::FixupRelativeFile(dir, file_part);
  EXPECT_TRUE(IsMatchingFileURL(fixedup, full_path));
  EXPECT_TRUE(file_util::Delete(full_path, false));

  // create a filename we know doesn't exist and make sure it doesn't get
  // fixed up to a file URL
  FilePath nonexistent_file(
      FILE_PATH_LITERAL("url_fixer_upper_nonexistent_file.txt"));
  fixedup = URLFixerUpper::FixupRelativeFile(dir, nonexistent_file);
  EXPECT_NE(std::string("file:///"), fixedup.substr(0, 8));
  EXPECT_FALSE(IsMatchingFileURL(fixedup, nonexistent_file));

  // make a subdir to make sure relative paths with directories work, also
  // test spaces:
  // "app_dir\url fixer-upper dir\url fixer-upper existing file.txt"
  FilePath sub_dir(FILE_PATH_LITERAL("url fixer-upper dir"));
  FilePath sub_file(FILE_PATH_LITERAL("url fixer-upper existing file.txt"));
  FilePath new_dir = dir.Append(sub_dir);
  file_util::CreateDirectory(new_dir);
  ASSERT_TRUE(MakeTempFile(new_dir, sub_file, &full_path));

  // test file in the subdir
  FilePath relative_file = sub_dir.Append(sub_file);
  fixedup = URLFixerUpper::FixupRelativeFile(dir, relative_file);
  EXPECT_TRUE(IsMatchingFileURL(fixedup, full_path));

  // test file in the subdir with different slashes and escaping.
  FilePath::StringType relative_file_str = sub_dir.value() +
      FILE_PATH_LITERAL("/") + sub_file.value();
  ReplaceSubstringsAfterOffset(&relative_file_str, 0,
      FILE_PATH_LITERAL(" "), FILE_PATH_LITERAL("%20"));
  fixedup = URLFixerUpper::FixupRelativeFile(dir, FilePath(relative_file_str));
  EXPECT_TRUE(IsMatchingFileURL(fixedup, full_path));

  // test relative directories and duplicate slashes
  // (should resolve to the same file as above)
  relative_file_str = sub_dir.value() + FILE_PATH_LITERAL("/../") +
      sub_dir.value() + FILE_PATH_LITERAL("///./") + sub_file.value();
  fixedup = URLFixerUpper::FixupRelativeFile(dir, FilePath(relative_file_str));
  EXPECT_TRUE(IsMatchingFileURL(fixedup, full_path));

  // done with the subdir
  EXPECT_TRUE(file_util::Delete(full_path, false));
  EXPECT_TRUE(file_util::Delete(new_dir, true));
}
