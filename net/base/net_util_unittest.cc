// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

namespace {

class NetUtilTest : public testing::Test {
};

struct FileCase {
  const wchar_t* file;
  const char* url;
};

struct HeaderCase {
  const wchar_t* header_name;
  const wchar_t* expected;
};

struct HeaderParamCase {
  const wchar_t* header_name;
  const wchar_t* param_name;
  const wchar_t* expected;
};

struct FileNameCDCase {
  const char* header_field;
  const char* referrer_charset;
  const wchar_t* expected;
};

const wchar_t* kLanguages[] = {
  L"",      L"en",    L"zh-CN",       L"ja",    L"ko",
  L"he",    L"ar",    L"ru",          L"el",    L"fr",
  L"de",    L"pt",    L"sv",          L"th",    L"hi",
  L"de,en", L"el,en", L"zh,zh-TW,en", L"ko,ja", L"he,ru,en",
  L"zh,ru,en"
};

struct IDNTestCase {
  const char* input;
  const wchar_t* unicode_output;
  const bool unicode_allowed[arraysize(kLanguages)];
};

// TODO(jungshik) This is just a random sample of languages and is far
// from exhaustive.  We may have to generate all the combinations
// of languages (powerset of a set of all the languages).
const IDNTestCase idn_cases[] = {
  // No IDN
  {"www.google.com", L"www.google.com",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"www.google.com.", L"www.google.com.",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {".", L".",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"", L"",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // IDN
  // Hanzi (Chinese)
  {"xn--1lq90i.cn", L"\x5317\x4eac.cn",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi + '123'
  {"www.xn--123-p18d.com", L"www.\x4e00" L"123.com",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi + Latin
  {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com",
   {false, false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Kanji + Kana (Japanese)
  {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp",
   {true,  false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  #if 0
  // U+30FC is not a part of the Japanese exemplar set.
  // Enable this after 'fixing' ICU data or locally working around it.
  // Katakana + Latin (Japanese)
  {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp",
   {true,   false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    }},
  #endif
  // Hangul (Korean)
  {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr",
   {true,  false, false, false, true,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // b<u-umlaut>cher (German)
  {"xn--bcher-kva.de", L"b\x00fc" L"cher.de",
   {true,  false, false, false, false,
    false, false, false, false, true,
    true,  true,  false, false, false,
    true,  false, false, false, false,
    false}},
  // a with diaeresis
  {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se",
   {true,  false, false, false, false,
    false, false, false, false, false,
    true,  false, true, false, false,
    true,  false, false, false, false,
    false}},
  // c-cedilla (French)
  {"www.xn--alliancefranaise-npb.fr", L"www.alliancefran\x00e7" L"aise.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // caf'e with acute accent' (French)
  {"xn--caf-dma.fr", L"caf\x00e9.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // c-cedillla and a with tilde (Portuguese)
  {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // s with caron
  {"xn--achy-f6a.com", L"\x0161" L"achy.com",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // TODO(jungshik) : Add examples with Cyrillic letters
  // only used in some languages written in Cyrillic.
  // Eutopia (Greek)
  {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Eutopia + 123 (Greek)
  {"xn---123-pldm0haj2bk.gr",
   L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Cyrillic (Russian)
  {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Cyrillic + 123 (Russian)
  {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Arabic
  {"xn--mgba1fmg.ar", L"\x0627\x0641\x0644\x0627\x0645.ar",
   {true,  false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Hebrew
  {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he",
   {true,  false, false, false, false,
    true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false}},
  // Thai
  {"xn--12c2cc4ag3b4ccu.th",
   L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false}},
  // Devangari (Hindi)
  {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false, false, false, false, false,
    false}},
  // Invalid IDN
  {"xn--hello?world.com", NULL,
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Unsafe IDNs
  // "payp<alpha>l.com"
  {"www.xn--paypl-g9d.com", L"payp\x03b1l.com",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.gr with Greek omicron and epsilon
  {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.ru with Cyrillic o
  {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // h<e with acute>llo<China in Han>.cn
  {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // <Greek rho><Cyrillic a><Cyrillic u>.ru
  {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // One that's really long that will force a buffer realloc
  {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaa",
   L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       L"aaaaaaaa",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // Test cases for characters we blacklisted although allowed in IDN.
  // Embedded spaces will be turned to %20 in the display.
  // TODO(jungshik): We need to have more cases. This is a typical
  // data-driven trap. The following test cases need to be separated
  // and tested only for a couple of languages.
  {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false}},
  {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comabc-k8d", L"google.com\x0338" L"abc",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     }},
#if 0
  // These two cases are special. We need a separate test.
  // U+3000 and U+3002 are normalized to ASCII space and dot.
  {"xn-- -kq6ay5z.cn", L"\x4e2d\x56fd\x3000.cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
  {"xn--fiqs8s.cn", L"\x4e2d\x56fd\x3002" L"cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
#endif
};

struct SuggestedFilenameCase {
  const char* url;
  const char* content_disp_header;
  const char* referrer_charset;
  const wchar_t* default_filename;
  const wchar_t* expected_filename;
};

struct UrlTestData {
  const char* description;
  const char* input;
  const std::wstring languages;
  bool omit;
  UnescapeRule::Type escape_rules;
  const std::wstring output;
  size_t prefix_len;
};

// Returns an addrinfo for the given 32-bit address (IPv4.)
// The result lives in static storage, so don't delete it.
// |bytes| should be an array of length 4.
const struct addrinfo* GetIPv4Address(const uint8* bytes) {
  static struct addrinfo static_ai;
  static struct sockaddr_in static_addr4;

  struct addrinfo* ai = &static_ai;
  ai->ai_socktype = SOCK_STREAM;
  memset(ai, 0, sizeof(static_ai));

  ai->ai_family = AF_INET;
  ai->ai_addrlen = sizeof(static_addr4);

  struct sockaddr_in* addr4 = &static_addr4;
  memset(addr4, 0, sizeof(static_addr4));
  addr4->sin_port = htons(80);
  addr4->sin_family = ai->ai_family;
  memcpy(&addr4->sin_addr, bytes, 4);

  ai->ai_addr = (sockaddr*)addr4;
  return ai;
}

// Returns a addrinfo for the given 128-bit address (IPv6.)
// The result lives in static storage, so don't delete it.
// |bytes| should be an array of length 16.
const struct addrinfo* GetIPv6Address(const uint8* bytes) {
  static struct addrinfo static_ai;
  static struct sockaddr_in6 static_addr6;

  struct addrinfo* ai = &static_ai;
  ai->ai_socktype = SOCK_STREAM;
  memset(ai, 0, sizeof(static_ai));

  ai->ai_family = AF_INET6;
  ai->ai_addrlen = sizeof(static_addr6);

  struct sockaddr_in6* addr6 = &static_addr6;
  memset(addr6, 0, sizeof(static_addr6));
  addr6->sin6_port = htons(80);
  addr6->sin6_family = ai->ai_family;
  memcpy(&addr6->sin6_addr, bytes, 16);

  ai->ai_addr = (sockaddr*)addr6;
  return ai;
}

}  // anonymous namespace

TEST(NetUtilTest, FileURLConversion) {
  // a list of test file names and the corresponding URLs
  const FileCase round_trip_cases[] = {
#if defined(OS_WIN)
    {L"C:\\foo\\bar.txt", "file:///C:/foo/bar.txt"},
    {L"\\\\some computer\\foo\\bar.txt",
     "file://some%20computer/foo/bar.txt"}, // UNC
    {L"D:\\Name;with%some symbols*#",
     "file:///D:/Name%3Bwith%25some%20symbols*%23"},
    // issue 14153: To be tested with the OS default codepage other than 1252.
    {L"D:\\latin1\\caf\x00E9\x00DD.txt",
     "file:///D:/latin1/caf%C3%A9%C3%9D.txt"},
    {L"D:\\otherlatin\\caf\x0119.txt",
     "file:///D:/otherlatin/caf%C4%99.txt"},
    {L"D:\\greek\\\x03B1\x03B2\x03B3.txt",
     "file:///D:/greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"D:\\Chinese\\\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///D:/Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD%91"
         "%E9%A1%B5.doc"},
    {L"D:\\plane1\\\xD835\xDC00\xD835\xDC01.txt",  // Math alphabet "AB"
     "file:///D:/plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
#elif defined(OS_POSIX)
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/BAR.txt", "file:///foo/BAR.txt"},
    {L"/C:/foo/bar.txt", "file:///C:/foo/bar.txt"},
    {L"/some computer/foo/bar.txt", "file:///some%20computer/foo/bar.txt"},
    {L"/Name;with%some symbols*#", "file:///Name%3Bwith%25some%20symbols*%23"},
    {L"/latin1/caf\x00E9\x00DD.txt", "file:///latin1/caf%C3%A9%C3%9D.txt"},
    {L"/otherlatin/caf\x0119.txt", "file:///otherlatin/caf%C4%99.txt"},
    {L"/greek/\x03B1\x03B2\x03B3.txt", "file:///greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"/Chinese/\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD"
         "%91%E9%A1%B5.doc"},
    {L"/plane1/\x1D400\x1D401.txt",  // Math alphabet "AB"
     "file:///plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
#endif
  };

  // First, we'll test that we can round-trip all of the above cases of URLs
  FilePath output;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(round_trip_cases); i++) {
    // convert to the file URL
    GURL file_url(net::FilePathToFileURL(
        FilePath::FromWStringHack(round_trip_cases[i].file)));
    EXPECT_EQ(round_trip_cases[i].url, file_url.spec());

    // Back to the filename.
    EXPECT_TRUE(net::FileURLToFilePath(file_url, &output));
    EXPECT_EQ(round_trip_cases[i].file, output.ToWStringHack());
  }

  // Test that various file: URLs get decoded into the correct file type
  FileCase url_cases[] = {
#if defined(OS_WIN)
    {L"C:\\foo\\bar.txt", "file:c|/foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:/c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:///c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file:////foo\\bar.txt"},
    {L"\\\\foo\\bar.txt", "file:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:\\\\\\c:/foo/bar.txt"},
#elif defined(OS_POSIX)
    {L"/c:/foo/bar.txt", "file:/c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:///c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:foo/bar.txt"},
    {L"/bar.txt", "file://foo/bar.txt"},
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo//bar.txt"},
    {L"/foo/bar.txt", "file:////foo///bar.txt"},
    {L"/foo/bar.txt", "file:////foo////bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:c:/foo/bar.txt"},
    // We get these wrong because GURL turns back slashes into forward
    // slashes.
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
    //{L"/c|/foo%5Cbar.txt", "file:c|/foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file:////foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(url_cases); i++) {
    net::FileURLToFilePath(GURL(url_cases[i].url), &output);
    EXPECT_EQ(url_cases[i].file, output.ToWStringHack());
  }

  // Unfortunately, UTF8ToWide discards invalid UTF8 input.
#ifdef BUG_878908_IS_FIXED
  // Test that no conversion happens if the UTF-8 input is invalid, and that
  // the input is preserved in UTF-8
  const char invalid_utf8[] = "file:///d:/Blah/\xff.doc";
  const wchar_t invalid_wide[] = L"D:\\Blah\\\xff.doc";
  EXPECT_TRUE(net::FileURLToFilePath(
      GURL(std::string(invalid_utf8)), &output));
  EXPECT_EQ(std::wstring(invalid_wide), output);
#endif

  // Test that if a file URL is malformed, we get a failure
  EXPECT_FALSE(net::FileURLToFilePath(GURL("filefoobar"), &output));
}

// Just a bunch of fake headers.
const wchar_t* google_headers =
    L"HTTP/1.1 200 OK\n"
    L"Content-TYPE: text/html; charset=utf-8\n"
    L"Content-disposition: attachment; filename=\"download.pdf\"\n"
    L"Content-Length: 378557\n"
    L"X-Google-Google1: 314159265\n"
    L"X-Google-Google2: aaaa2:7783,bbb21:9441\n"
    L"X-Google-Google4: home\n"
    L"Transfer-Encoding: chunked\n"
    L"Set-Cookie: HEHE_AT=6666x66beef666x6-66xx6666x66; Path=/mail\n"
    L"Set-Cookie: HEHE_HELP=owned:0;Path=/\n"
    L"Set-Cookie: S=gmail=Xxx-beefbeefbeef_beefb:gmail_yj=beefbeef000beefbee"
        L"fbee:gmproxy=bee-fbeefbe; Domain=.google.com; Path=/\n"
    L"X-Google-Google2: /one/two/three/four/five/six/seven-height/nine:9411\n"
    L"Server: GFE/1.3\n"
    L"Transfer-Encoding: chunked\n"
    L"Date: Mon, 13 Nov 2006 21:38:09 GMT\n"
    L"Expires: Tue, 14 Nov 2006 19:23:58 GMT\n"
    L"X-Malformed: bla; arg=test\"\n"
    L"X-Malformed2: bla; arg=\n"
    L"X-Test: bla; arg1=val1; arg2=val2";

TEST(NetUtilTest, GetSpecificHeader) {
  const HeaderCase tests[] = {
    {L"content-type", L"text/html; charset=utf-8"},
    {L"CONTENT-LENGTH", L"378557"},
    {L"Date", L"Mon, 13 Nov 2006 21:38:09 GMT"},
    {L"Bad-Header", L""},
    {L"", L""},
  };

  // Test first with google_headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::wstring result = net::GetSpecificHeader(google_headers,
                                                 tests[i].header_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  // Test again with empty headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::wstring result = net::GetSpecificHeader(L"", tests[i].header_name);
    EXPECT_EQ(result, std::wstring());
  }
}

TEST(NetUtilTest, GetHeaderParamValue) {
  const HeaderParamCase tests[] = {
    {L"Content-type", L"charset", L"utf-8"},
    {L"content-disposition", L"filename", L"download.pdf"},
    {L"Content-Type", L"badparam", L""},
    {L"X-Malformed", L"arg", L"test\""},
    {L"X-Malformed2", L"arg", L""},
    {L"X-Test", L"arg1", L"val1"},
    {L"X-Test", L"arg2", L"val2"},
    {L"Bad-Header", L"badparam", L""},
    {L"Bad-Header", L"", L""},
    {L"", L"badparam", L""},
    {L"", L"", L""},
  };
  // TODO(mpcomplete): add tests for other formats of headers.

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::wstring header_value =
        net::GetSpecificHeader(google_headers, tests[i].header_name);
    std::wstring result =
        net::GetHeaderParamValue(header_value, tests[i].param_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::wstring header_value =
        net::GetSpecificHeader(L"", tests[i].header_name);
    std::wstring result =
        net::GetHeaderParamValue(header_value, tests[i].param_name);
    EXPECT_EQ(result, std::wstring());
  }
}

TEST(NetUtilTest, GetFileNameFromCD) {
  const FileNameCDCase tests[] = {
    // Test various forms of C-D header fields emitted by web servers.
    {"content-disposition: inline; filename=\"abcde.pdf\"", "", L"abcde.pdf"},
    {"content-disposition: inline; name=\"abcde.pdf\"", "", L"abcde.pdf"},
    {"content-disposition: attachment; filename=abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: attachment; name=abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: attachment; filename=abc,de.pdf", "", L"abc,de.pdf"},
    {"content-disposition: filename=abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: filename= abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: filename =abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: filename = abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: filename\t=abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: filename \t\t  =abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: name=abcde.pdf", "", L"abcde.pdf"},
    {"content-disposition: inline; filename=\"abc%20de.pdf\"", "",
     L"abc de.pdf"},
    // Whitespaces are converted to a space.
    {"content-disposition: inline; filename=\"abc  \t\nde.pdf\"", "",
     L"abc    de.pdf"},
    // %-escaped UTF-8
    {"Content-Disposition: attachment; filename=\"%EC%98%88%EC%88%A0%20"
     "%EC%98%88%EC%88%A0.jpg\"", "", L"\xc608\xc220 \xc608\xc220.jpg"},
    {"Content-Disposition: attachment; filename=\"%F0%90%8C%B0%F0%90%8C%B1"
     "abc.jpg\"", "", L"\U00010330\U00010331abc.jpg"},
    {"Content-Disposition: attachment; filename=\"%EC%98%88%EC%88%A0 \n"
     "%EC%98%88%EC%88%A0.jpg\"", "", L"\xc608\xc220  \xc608\xc220.jpg"},
    // RFC 2047 with various charsets and Q/B encodings
    {"Content-Disposition: attachment; filename=\"=?EUC-JP?Q?=B7=DD=BD="
     "D13=2Epng?=\"", "", L"\x82b8\x8853" L"3.png"},
    {"Content-Disposition: attachment; filename==?eUc-Kr?b?v7m8+iAzLnBuZw==?=",
     "", L"\xc608\xc220 3.png"},
    {"Content-Disposition: attachment; filename==?utf-8?Q?=E8=8A=B8=E8"
     "=A1=93_3=2Epng?=", "", L"\x82b8\x8853 3.png"},
    {"Content-Disposition: attachment; filename==?utf-8?Q?=F0=90=8C=B0"
     "_3=2Epng?=", "", L"\U00010330 3.png"},
    {"Content-Disposition: inline; filename=\"=?iso88591?Q?caf=e9_=2epng?=\"",
     "", L"caf\x00e9 .png"},
    // Space after an encode word should be removed.
    {"Content-Disposition: inline; filename=\"=?iso88591?Q?caf=E9_?= .png\"",
     "", L"caf\x00e9 .png"},
    // Two encoded words with different charsets (not very likely to be emitted
    // by web servers in the wild). Spaces between them are removed.
    {"Content-Disposition: inline; filename=\"=?euc-kr?b?v7m8+iAz?="
     " =?ksc5601?q?=BF=B9=BC=FA=2Epng?=\"", "",
     L"\xc608\xc220 3\xc608\xc220.png"},
    {"Content-Disposition: attachment; filename=\"=?windows-1252?Q?caf=E9?="
     "  =?iso-8859-7?b?4eI=?= .png\"", "", L"caf\x00e9\x03b1\x03b2.png"},
    // Non-ASCII string is passed through and treated as UTF-8 as long as
    // it's valid as UTF-8 and regardless of |referrer_charset|.
    {"Content-Disposition: attachment; filename=caf\xc3\xa9.png",
     "iso-8859-1", L"caf\x00e9.png"},
    {"Content-Disposition: attachment; filename=caf\xc3\xa9.png",
     "", L"caf\x00e9.png"},
    // Non-ASCII/Non-UTF-8 string. Fall back to the referrer charset.
    {"Content-Disposition: attachment; filename=caf\xe5.png",
     "windows-1253", L"caf\x03b5.png"},
#if 0
    // Non-ASCII/Non-UTF-8 string. Fall back to the native codepage.
    // TODO(jungshik): We need to set the OS default codepage
    // to a specific value before testing. On Windows, we can use
    // SetThreadLocale().
    {"Content-Disposition: attachment; filename=\xb0\xa1\xb0\xa2.png",
     "", L"\xac00\xac01.png"},
#endif
    // Failure cases
    // Invalid hex-digit "G"
    {"Content-Disposition: attachment; filename==?iiso88591?Q?caf=EG?=", "",
     L""},
    // Incomplete RFC 2047 encoded-word (missing '='' at the end)
    {"Content-Disposition: attachment; filename==?iso88591?Q?caf=E3?", "", L""},
    // Extra character at the end of an encoded word
    {"Content-Disposition: attachment; filename==?iso88591?Q?caf=E3?==",
     "", L""},
    // Extra token at the end of an encoded word
    {"Content-Disposition: attachment; filename==?iso88591?Q?caf=E3?=?",
     "", L""},
    {"Content-Disposition: attachment; filename==?iso88591?Q?caf=E3?=?=",
     "",  L""},
    // Incomplete hex-escaped chars
    {"Content-Disposition: attachment; filename==?windows-1252?Q?=63=61=E?=",
     "", L""},
    {"Content-Disposition: attachment; filename=%EC%98%88%EC%88%A", "", L""},
    // %-escaped non-UTF-8 encoding is an "error"
    {"Content-Disposition: attachment; filename=%B7%DD%BD%D1.png", "", L""},
    // Two RFC 2047 encoded words in a row without a space is an error.
    {"Content-Disposition: attachment; filename==?windows-1252?Q?caf=E3?="
     "=?iso-8859-7?b?4eIucG5nCg==?=", "", L""},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(tests[i].expected,
              net::GetFileNameFromCD(tests[i].header_field,
                                     tests[i].referrer_charset));
  }
}

TEST(NetUtilTest, IDNToUnicodeFast) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // ja || zh,zh-TW,en || ko,ja -> IDNToUnicodeSlow
      if (j == 3 || j == 17 || j == 18)
        continue;
      std::wstring output;
      net::IDNToUnicode(idn_cases[i].input,
                        static_cast<int>(strlen(idn_cases[i].input)),
                        kLanguages[j],
                        &output);
      std::wstring expected(idn_cases[i].unicode_allowed[j] ?
                            idn_cases[i].unicode_output :
                            ASCIIToWide(idn_cases[i].input));
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, IDNToUnicodeSlow) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // !(ja || zh,zh-TW,en || ko,ja) -> IDNToUnicodeFast
      if (!(j == 3 || j == 17 || j == 18))
        continue;
      std::wstring output;
      net::IDNToUnicode(idn_cases[i].input,
                        static_cast<int>(strlen(idn_cases[i].input)),
                        kLanguages[j],
                        &output);
      std::wstring expected(idn_cases[i].unicode_allowed[j] ?
                            idn_cases[i].unicode_output :
                            ASCIIToWide(idn_cases[i].input));
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, StripWWW) {
  EXPECT_EQ(L"", net::StripWWW(L""));
  EXPECT_EQ(L"", net::StripWWW(L"www."));
  EXPECT_EQ(L"blah", net::StripWWW(L"www.blah"));
  EXPECT_EQ(L"blah", net::StripWWW(L"blah"));
}

TEST(NetUtilTest, GetSuggestedFilename) {
  const SuggestedFilenameCase test_cases[] = {
    {"http://www.google.com/",
     "Content-disposition: attachment; filename=test.html",
     "",
     L"",
     L"test.html"},
    {"http://www.google.com/",
     "Content-disposition: attachment; filename=\"test.html\"",
     "",
     L"",
     L"test.html"},
    {"http://www.google.com/path/test.html",
     "Content-disposition: attachment",
     "",
     L"",
     L"test.html"},
    {"http://www.google.com/path/test.html",
     "Content-disposition: attachment;",
     "",
     L"",
     L"test.html"},
    {"http://www.google.com/",
     "",
     "",
     L"",
     L"www.google.com"},
    {"http://www.google.com/test.html",
     "",
     "",
     L"",
     L"test.html"},
    // Now that we use googleurl's ExtractFileName, this case falls back
    // to the hostname. If this behavior is not desirable, we'd better
    // change ExtractFileName (in url_parse).
    {"http://www.google.com/path/",
     "",
     "",
     L"",
     L"www.google.com"},
    {"http://www.google.com/path",
     "",
     "",
     L"",
     L"path"},
    {"file:///",
     "",
     "",
     L"",
     L"download"},
    {"view-cache:",
     "",
     "",
     L"",
     L"download"},
    {"http://www.google.com/",
     "Content-disposition: attachment; filename =\"test.html\"",
     "",
     L"download",
     L"test.html"},
    {"http://www.google.com/",
     "",
     "",
     L"download",
     L"download"},
    {"http://www.google.com/",
     "Content-disposition: attachment; filename=\"../test.html\"",
     "",
     L"",
     L"test.html"},
    {"http://www.google.com/",
     "Content-disposition: attachment; filename=\"..\"",
     "",
     L"download",
     L"download"},
    {"http://www.google.com/test.html",
     "Content-disposition: attachment; filename=\"..\"",
     "",
     L"download",
     L"test.html"},
    // Below is a small subset of cases taken from GetFileNameFromCD test above.
    {"http://www.google.com/",
     "Content-Disposition: attachment; filename=\"%EC%98%88%EC%88%A0%20"
     "%EC%98%88%EC%88%A0.jpg\"",
     "",
     L"",
     L"\uc608\uc220 \uc608\uc220.jpg"},
    {"http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg",
     "",
     "",
     L"download",
     L"\uc608\uc220 \uc608\uc220.jpg"},
    {"http://www.google.com/",
     "Content-disposition: attachment;",
     "",
     L"\uB2E4\uC6B4\uB85C\uB4DC",
     L"\uB2E4\uC6B4\uB85C\uB4DC"},
    {"http://www.google.com/",
     "Content-Disposition: attachment; filename=\"=?EUC-JP?Q?=B7=DD=BD="
     "D13=2Epng?=\"",
     "",
     L"download",
     L"\u82b8\u88533.png"},
    {"http://www.example.com/images?id=3",
     "Content-Disposition: attachment; filename=caf\xc3\xa9.png",
     "iso-8859-1",
     L"",
     L"caf\u00e9.png"},
    {"http://www.example.com/images?id=3",
     "Content-Disposition: attachment; filename=caf\xe5.png",
     "windows-1253",
     L"",
     L"caf\u03b5.png"},
    {"http://www.example.com/file?id=3",
     "Content-Disposition: attachment; name=\xcf\xc2\xd4\xd8.zip",
     "GBK",
     L"",
     L"\u4e0b\u8f7d.zip"},
    // Invalid C-D header. Extracts filename from url.
    {"http://www.google.com/test.html",
     "Content-Disposition: attachment; filename==?iiso88591?Q?caf=EG?=",
     "",
     L"",
     L"test.html"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::wstring filename = net::GetSuggestedFilename(
        GURL(test_cases[i].url), test_cases[i].content_disp_header,
        test_cases[i].referrer_charset, test_cases[i].default_filename);
    EXPECT_EQ(std::wstring(test_cases[i].expected_filename), filename);
  }
}

// This is currently a windows specific function.
#if defined(OS_WIN)
namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* raw_bytes;
  bool is_dir;
  int64 filesize;
  base::Time time;
  const char* expected;
};

}  // namespace
TEST(NetUtilTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
    {L"Foo",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"Foo\",\"Foo\",0,\"9.8 kB\",\"\");</script>\n"},
    {L"quo\"tes",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    {L"quo\"tes",
     "quo\"tes",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
    // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
    {L"\xD55C\xAE00.txt",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\\uD55C\\uAE00.txt\",\"%ED%95%9C%EA%B8%80.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
    // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
    // a local or remote file in EUC-KR.
    {L"\xD55C\xAE00.txt",
     "\xC7\xD1\xB1\xDB.txt",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\\uD55C\\uAE00.txt\",\"%C7%D1%B1%DB.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const std::string results = net::GetDirectoryListingEntry(
        WideToUTF16(test_cases[i].name),
        test_cases[i].raw_bytes,
        test_cases[i].is_dir,
        test_cases[i].filesize,
        test_cases[i].time);
    EXPECT_EQ(test_cases[i].expected, results);
  }
}

#endif

TEST(NetUtilTest, ParseHostAndPort) {
  const struct {
    const char* input;
    bool success;
    const char* expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "[1080:0:0:0:8:800:200C:4171]",
      11,
    },
    // Invalid inputs:
    {"foo:bar", false, "", -1},
    {"foo:", false, "", -1},
    {":", false, "", -1},
    {":80", false, "", -1},
    {"", false, "", -1},
    {"porttoolong:300000", false, "", -1},
    {"usrname@host", false, "", -1},
    {"usrname:password@host", false, "", -1},
    {":password@host", false, "", -1},
    {":password@host:80", false, "", -1},
    {":password@host", false, "", -1},
    {"@host", false, "", -1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host;
    int port;
    bool ok = net::ParseHostAndPort(tests[i].input, &host, &port);

    EXPECT_EQ(tests[i].success, ok);

    if (tests[i].success) {
      EXPECT_EQ(tests[i].expected_host, host);
      EXPECT_EQ(tests[i].expected_port, port);
    }
  }
}

TEST(NetUtilTest, GetHostAndPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = net::GetHostAndPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = net::GetHostAndOptionalPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}


TEST(NetUtilTest, NetAddressToString_IPv4) {
  const struct {
    uint8 addr[4];
    const char* result;
  } tests[] = {
    {{0, 0, 0, 0}, "0.0.0.0"},
    {{127, 0, 0, 1}, "127.0.0.1"},
    {{192, 168, 0, 1}, "192.168.0.1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    const addrinfo* ai = GetIPv4Address(tests[i].addr);
    std::string result = net::NetAddressToString(ai);
    EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv6) {
  const struct {
    uint8 addr[16];
    const char* result;
  } tests[] = {
    {{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10},
     "fedc:ba98:7654:3210:fedc:ba98:7654:3210"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    const addrinfo* ai = GetIPv6Address(tests[i].addr);
    std::string result = net::NetAddressToString(ai);
    // Allow NetAddressToString() to fail, in case the system doesn't
    // support IPv6.
    if (!result.empty())
      EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = net::GetHostName();
  EXPECT_FALSE(hostname.empty());
}

TEST(NetUtilTest, FormatUrl) {
  const UrlTestData tests[] = {
    {"Empty URL", "", L"", true, UnescapeRule::NORMAL, L"", 0},

    {"Simple URL",
     "http://www.google.com/", L"", true, UnescapeRule::NORMAL,
     L"http://www.google.com/", 7},

    {"With a port number and a reference",
     "http://www.google.com:8080/#\xE3\x82\xB0", L"", true,
     UnescapeRule::NORMAL,
     L"http://www.google.com:8080/#\x30B0", 7},

    // -------- IDN tests --------
    {"Japanese IDN with ja",
     "http://xn--l8jvb1ey91xtjb.jp", L"ja", true, UnescapeRule::NORMAL,
     L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"Japanese IDN with en",
     "http://xn--l8jvb1ey91xtjb.jp", L"en", true, UnescapeRule::NORMAL,
     L"http://xn--l8jvb1ey91xtjb.jp/", 7},

    {"Japanese IDN without any languages",
     "http://xn--l8jvb1ey91xtjb.jp", L"", true, UnescapeRule::NORMAL,
     // Single script is safe for empty languages.
     L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"mailto: with Japanese IDN",
     "mailto:foo@xn--l8jvb1ey91xtjb.jp", L"ja", true, UnescapeRule::NORMAL,
     // GURL doesn't assume an email address's domain part as a host name.
     L"mailto:foo@xn--l8jvb1ey91xtjb.jp", 7},

    {"file: with Japanese IDN",
     "file://xn--l8jvb1ey91xtjb.jp/config.sys", L"ja", true,
     UnescapeRule::NORMAL,
     L"file://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 7},

    {"ftp: with Japanese IDN",
     "ftp://xn--l8jvb1ey91xtjb.jp/config.sys", L"ja", true,
     UnescapeRule::NORMAL,
     L"ftp://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 6},

    // -------- omit_username_password flag tests --------
    {"With username and password, omit_username_password=false",
     "http://user:passwd@example.com/foo", L"", false, UnescapeRule::NORMAL,
     L"http://user:passwd@example.com/foo", 19},

    {"With username and password, omit_username_password=true",
     "http://user:passwd@example.com/foo", L"", true, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    {"With username and no password",
     "http://user@example.com/foo", L"", true, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    {"Just '@' without username and password",
     "http://@example.com/foo", L"", true, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    // GURL doesn't think local-part of an email address is username for URL.
    {"mailto:, omit_username_password=true",
     "mailto:foo@example.com", L"", true, UnescapeRule::NORMAL,
     L"mailto:foo@example.com", 7},

    // -------- unescape flag tests --------
    {"Do not unescape",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", L"en", true,
     UnescapeRule::NONE,
     // GURL parses %-encoded hostnames into Punycode.
     L"http://xn--qcka1pmc.jp/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     L"?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", 7},

    {"Unescape normally",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", L"en", true,
     UnescapeRule::NORMAL,
     L"http://xn--qcka1pmc.jp/\x30B0\x30FC\x30B0\x30EB"
     L"?q=\x30B0\x30FC\x30B0\x30EB", 7},

    {"Unescape normally including unescape spaces",
     "http://www.google.com/search?q=Hello%20World", L"en", true,
     UnescapeRule::SPACES,
     L"http://www.google.com/search?q=Hello World", 7},

    /*
    {"unescape=true with some special characters",
    "http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", L"", false, true,
    L"http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", 25},
    */
    // Disabled: the resultant URL becomes "...user%253A:%2540passwd...".

    // -------- view-source: --------
    {"view-source",
     "view-source:http://xn--qcka1pmc.jp/", L"ja", true, UnescapeRule::NORMAL,
     L"view-source:http://\x30B0\x30FC\x30B0\x30EB.jp/", 12 + 7},

    {"view-source of view-source",
     "view-source:view-source:http://xn--qcka1pmc.jp/", L"ja", true,
     UnescapeRule::NORMAL,
     L"view-source:view-source:http://xn--qcka1pmc.jp/", 12},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    size_t prefix_len;
    std::wstring formatted = net::FormatUrl(
        GURL(tests[i].input), tests[i].languages, tests[i].omit,
        tests[i].escape_rules, NULL, &prefix_len);
    EXPECT_EQ(tests[i].output, formatted) << tests[i].description;
    EXPECT_EQ(tests[i].prefix_len, prefix_len) << tests[i].description;
  }
}

TEST(NetUtilTest, FormatUrlParsed) {
  // No unescape case.
  url_parse::Parsed parsed;
  std::wstring formatted = net::FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      L"ja", false, UnescapeRule::NONE, &parsed, NULL);
  EXPECT_EQ(L"http://%E3%82%B0:%E3%83%BC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/%E3%82%B0/?q=%E3%82%B0#\x30B0", formatted);
  EXPECT_EQ(L"%E3%82%B0",
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(L"%E3%83%BC",
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(L"\x30B0\x30FC\x30B0\x30EB.jp",
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(L"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(L"/%E3%82%B0/",
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(L"q=%E3%82%B0",
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(L"\x30B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Unescape case.
  formatted = net::FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      L"ja", false, UnescapeRule::NORMAL, &parsed, NULL);
  EXPECT_EQ(L"http://\x30B0:\x30FC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0", formatted);
  EXPECT_EQ(L"\x30B0",
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(L"\x30FC",
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(L"\x30B0\x30FC\x30B0\x30EB.jp",
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(L"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(L"/\x30B0/", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(L"q=\x30B0",
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(L"\x30B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Omit_username_password + unescape case.
  formatted = net::FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      L"ja", true, UnescapeRule::NORMAL, &parsed, NULL);
  EXPECT_EQ(L"http://\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0", formatted);
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(L"\x30B0\x30FC\x30B0\x30EB.jp",
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(L"8080", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(L"/\x30B0/", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(L"q=\x30B0",
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(L"\x30B0", formatted.substr(parsed.ref.begin, parsed.ref.len));

  // View-source case.
  formatted = net::FormatUrl(
      GURL("view-source:http://user:passwd@host:81/path?query#ref"),
      L"", true, UnescapeRule::NORMAL, &parsed, NULL);
  EXPECT_EQ(L"view-source:http://host:81/path?query#ref", formatted);
  EXPECT_EQ(L"view-source:http",
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(L"host", formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(L"81", formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(L"/path", formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(L"query", formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(L"ref", formatted.substr(parsed.ref.begin, parsed.ref.len));
}
