// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

// Basic tests that verify our KURL's interface behaves the same as the
// original KURL's.

#include "config.h"

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_util.h"

#include "KURL.h"

#undef LOG
#include "base/basictypes.h"
#include "base/string16.h"
#include "base/string_util.h"

namespace {

struct ComponentCase {
  const char* url;
  const char* protocol;
  const char* host;
  const int port;
  const char* user;
  const char* pass;
  const char* path;
  const char* last_path;
  const char* query;
  const char* ref;
};

// Output stream operator so gTest's macros work with WebCore strings.
std::ostream& operator<<(std::ostream& out,
                         const WebCore::String& str) {
  if (str.isEmpty())
    return out;
  return out << WideToUTF8(std::wstring(
    reinterpret_cast<const wchar_t*>(str.characters()), str.length()));
}

}  // namespace

// Test the cases where we should be the same as WebKit's old KURL.
TEST(GKURL, SameGetters) {
  struct GetterCase {
    const char* url;
    const char* protocol;
    const char* host;
    int port;
    const char* user;
    const char* pass;
    const char* last_path_component;
    const char* query;
    const char* ref;
    bool has_ref;
  } cases[] = {
    {"http://www.google.com/foo/blah?bar=baz#ref", "http", "www.google.com", 0, "", NULL, "blah", "?bar=baz", "ref", true},
    {"http://foo.com:1234/foo/bar/", "http", "foo.com", 1234, "", NULL, "bar", "", NULL, false},
    {"http://www.google.com?#", "http", "www.google.com", 0, "", NULL, NULL, "?", "", true},
    {"https://me:pass@google.com:23#foo", "https", "google.com", 23, "me", "pass", NULL, "", "foo", true},
    {"javascript:hello!//world", "javascript", "", 0, "", NULL, "world", "", NULL, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    // UTF-8
    WebCore::KURL gurl(cases[i].url);

    EXPECT_EQ(cases[i].protocol, gurl.protocol());
    EXPECT_EQ(cases[i].host, gurl.host());
    EXPECT_EQ(cases[i].port, gurl.port());
    EXPECT_EQ(cases[i].user, gurl.user());
    EXPECT_EQ(cases[i].pass, gurl.pass());
    EXPECT_EQ(cases[i].last_path_component, gurl.lastPathComponent());
    EXPECT_EQ(cases[i].query, gurl.query());
    EXPECT_EQ(cases[i].ref, gurl.ref());
    EXPECT_EQ(cases[i].has_ref, gurl.hasRef());

    // UTF-16
    string16 wstr(UTF8ToUTF16(cases[i].url));
    WebCore::String utf16(
        reinterpret_cast<const ::UChar*>(wstr.c_str()),
        static_cast<int>(wstr.length()));
    gurl = WebCore::KURL(utf16);

    EXPECT_EQ(cases[i].protocol, gurl.protocol());
    EXPECT_EQ(cases[i].host, gurl.host());
    EXPECT_EQ(cases[i].port, gurl.port());
    EXPECT_EQ(cases[i].user, gurl.user());
    EXPECT_EQ(cases[i].pass, gurl.pass());
    EXPECT_EQ(cases[i].last_path_component, gurl.lastPathComponent());
    EXPECT_EQ(cases[i].query, gurl.query());
    EXPECT_EQ(cases[i].ref, gurl.ref());
    EXPECT_EQ(cases[i].has_ref, gurl.hasRef());
  }
}

// Test a few cases where we're different just to make sure we give reasonable
// output.
TEST(GKURL, DifferentGetters) {
  ComponentCase cases[] = {
     // url                                   protocol      host        port  user  pass    path                last_path query      ref

    // Old WebKit allows references and queries in what we call "path" URLs
    // like javascript, so the path here will only consist of "hello!".
    {"javascript:hello!?#/\\world",           "javascript", "",         0,    "",   NULL,   "hello!?#/\\world", "world",  "",      NULL},

    // Old WebKit doesn't handle "parameters" in paths, so will
    // disagree with us about where the path is for this URL.
    {"http://a.com/hello;world",              "http",       "a.com",    0,    "",   NULL,     "/hello;world",     "hello",  "",    NULL},

    // WebKit doesn't like UTF-8 or UTF-16 input.
    {"http://\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd/", "http", "xn--6qqa088eba", 0, "", NULL, "/",   NULL,     "",       NULL},

    // WebKit %-escapes non-ASCII characters in reference, but we don't.
    {"http://www.google.com/foo/blah?bar=baz#\xce\xb1\xce\xb2", "http", "www.google.com", 0, "", NULL, "/foo/blah/", "blah", "?bar=baz", "\xce\xb1\xce\xb2"}
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    WebCore::KURL gurl(cases[i].url);

    EXPECT_EQ(cases[i].protocol, gurl.protocol());
    EXPECT_EQ(cases[i].host, gurl.host());
    EXPECT_EQ(cases[i].port, gurl.port());
    EXPECT_EQ(cases[i].user, gurl.user());
    EXPECT_EQ(cases[i].pass, gurl.pass());
    EXPECT_EQ(cases[i].last_path, gurl.lastPathComponent());
    EXPECT_EQ(cases[i].query, gurl.query());
    // Want to compare UCS-16 refs (or to NULL).
    if (cases[i].ref) {
      EXPECT_EQ(webkit_glue::StdWStringToString(UTF8ToWide(cases[i].ref)),
                gurl.ref());
    } else {
      EXPECT_TRUE(gurl.ref().isNull());
    }
  }
}

// Ensures that both ASCII and UTF-8 canonical URLs are handled properly and we
// get the correct string object out.
TEST(GKURL, UTF8) {
  const char ascii_url[] = "http://foo/bar#baz";
  WebCore::KURL ascii_gurl(ascii_url);
  EXPECT_TRUE(ascii_gurl.string() == WebCore::String(ascii_url));

  // When the result is ASCII, we should get an ASCII String. Some
  // code depends on being able to compare the result of the .string()
  // getter with another String, and the isASCIIness of the two
  // strings must match for these functions (like equalIgnoringCase).
  EXPECT_TRUE(WebCore::equalIgnoringCase(ascii_gurl,
                                         WebCore::String(ascii_url)));

  // Reproduce code path in FrameLoader.cpp -- equalIgnoringCase implicitly
  // expects gkurl.protocol() to have been created as ascii.
  WebCore::KURL mailto("mailto:foo@foo.com");
  EXPECT_TRUE(WebCore::equalIgnoringCase(mailto.protocol(), "mailto"));

  const char utf8_url[] = "http://foo/bar#\xe4\xbd\xa0\xe5\xa5\xbd";
  WebCore::KURL utf8_gurl(utf8_url);

  EXPECT_TRUE(utf8_gurl.string() ==
              webkit_glue::StdWStringToString(UTF8ToWide(utf8_url)));
}

TEST(GKURL, Setters) {
  // Replace the starting URL with the given components one at a time and
  // verify that we're always the same as the old KURL.
  //
  // Note that old KURL won't canonicalize the default port away, so we
  // can't set setting the http port to "80" (or even "0").
  //
  // We also can't test clearing the query.
  //
  // The format is every other row is a test, and the row that follows it is the
  // expected result.
  struct ExpectedComponentCase {
    const char* url;
    const char* protocol;
    const char* host;
    const int port;
    const char* user;
    const char* pass;
    const char* path;
    const char* query;
    const char* ref;

    // The full expected URL with the given "set" applied.
    const char* expected_protocol;
    const char* expected_host;
    const char* expected_port;
    const char* expected_user;
    const char* expected_pass;
    const char* expected_path;
    const char* expected_query;
    const char* expected_ref;
  } cases[] = {
     // url                                   protocol      host               port  user  pass    path            query      ref
    {"http://www.google.com/",                "https",      "news.google.com", 8888, "me", "pass", "/foo",         "?q=asdf", "heehee",
                                              "https://www.google.com/",
                                                            "https://news.google.com/",
                                                                               "https://news.google.com:8888/",
                                                                                     "https://me@news.google.com:8888/",
                                                                                           "https://me:pass@news.google.com:8888/",
                                                                                                   "https://me:pass@news.google.com:8888/foo",
                                                                                                                   "https://me:pass@news.google.com:8888/foo?q=asdf",
                                                                                                                              "https://me:pass@news.google.com:8888/foo?q=asdf#heehee"},

    {"https://me:pass@google.com:88/a?f#b",   "http",       "goo.com",         92,   "",   "",     "/",            NULL,      "",
                                              "http://me:pass@google.com:88/a?f#b",
                                                            "http://me:pass@goo.com:88/a?f#b",
                                                                               "http://me:pass@goo.com:92/a?f#b",
                                                                                     "http://:pass@goo.com:92/a?f#b",
                                                                                           "http://goo.com:92/a?f#b",
                                                                                                    "http://goo.com:92/?f#b",
                                                                                                                   "http://goo.com:92/#b",
                                                                                                                              "https://goo.com:92/"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    WebCore::KURL gurl(cases[i].url);

    gurl.setProtocol(cases[i].protocol);
    EXPECT_STREQ(cases[i].expected_protocol, gurl.string().utf8().data());

    gurl.setHost(cases[i].host);
    EXPECT_STREQ(cases[i].expected_host, gurl.string().utf8().data());

    gurl.setPort(cases[i].port);
    EXPECT_STREQ(cases[i].expected_port, gurl.string().utf8().data());

    gurl.setUser(cases[i].user);
    EXPECT_STREQ(cases[i].expected_user, gurl.string().utf8().data());

    gurl.setPass(cases[i].pass);
    EXPECT_STREQ(cases[i].expected_pass, gurl.string().utf8().data());

    gurl.setPath(cases[i].path);
    EXPECT_STREQ(cases[i].expected_path, gurl.string().utf8().data());

    gurl.setQuery(cases[i].query);
    EXPECT_STREQ(cases[i].expected_query, gurl.string().utf8().data());

    // Refs are tested below. On the Safari 3.1 branch, we don't match their
    // KURL since we integrated a fix from their trunk.
  }
}

// Tests that KURL::decodeURLEscapeSequences works as expected
#if USE(GOOGLEURL)
TEST(GKURL, Decode) {
  struct DecodeCase {
    const char* input;
    const char* output;
  } decode_cases[] = {
    {"hello, world", "hello, world"},
    {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/", "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
    {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/", "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
    {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/", " !\"#$%&'()*+,-.//"},
    {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/", "0123456789:;<=>?/"},
    {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/", "@ABCDEFGHIJKLMNO/"},
    {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/", "PQRSTUVWXYZ[\\]^_/"},
    {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/", "`abcdefghijklmno/"},
    {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/", "pqrstuvwxyz{|}~\x7f/"},
      // Test un-UTF-8-ization.
    {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(decode_cases); i++) {
    WebCore::String input(decode_cases[i].input);
    WebCore::String str = WebCore::decodeURLEscapeSequences(input);
    EXPECT_STREQ(decode_cases[i].output, str.utf8().data());
  }

  // Our decode should not decode %00
  WebCore::String zero = WebCore::decodeURLEscapeSequences("%00");
  EXPECT_STREQ("%00", zero.utf8().data());

  // Test the error behavior for invalid UTF-8 (we differ from WebKit here).
  WebCore::String invalid = WebCore::decodeURLEscapeSequences(
      "%e4%a0%e5%a5%bd");
  char16 invalid_expected_helper[4] = { 0x00e4, 0x00a0, 0x597d, 0 };
  WebCore::String invalid_expected(
      reinterpret_cast<const ::UChar*>(invalid_expected_helper),
      3);
  EXPECT_EQ(invalid_expected, invalid);
}
#endif

TEST(GKURL, Encode) {
  // Also test that it gets converted to UTF-8 properly.
  char16 wide_input_helper[3] = { 0x4f60, 0x597d, 0 };
  WebCore::String wide_input(
      reinterpret_cast<const ::UChar*>(wide_input_helper), 2);
  WebCore::String wide_reference("\xe4\xbd\xa0\xe5\xa5\xbd", 6);
  WebCore::String wide_output =
      WebCore::encodeWithURLEscapeSequences(wide_input);
  EXPECT_EQ(wide_reference, wide_output);

  // Our encode only escapes NULLs for safety (see the implementation for
  // more), so we only bother to test a few cases.
  WebCore::String input(
      "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 16);
  WebCore::String reference(
      "%00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 18);
  WebCore::String output = WebCore::encodeWithURLEscapeSequences(input);
  EXPECT_EQ(reference, output);
}

TEST(GKURL, ResolveEmpty) {
  WebCore::KURL empty_base;

  // WebKit likes to be able to resolve absolute input agains empty base URLs,
  // which would normally be invalid since the base URL is invalid.
  const char abs[] = "http://www.google.com/";
  WebCore::KURL resolve_abs(empty_base, abs);
  EXPECT_TRUE(resolve_abs.isValid());
  EXPECT_STREQ(abs, resolve_abs.string().utf8().data());

  // Resolving a non-relative URL agains the empty one should still error.
  const char rel[] = "foo.html";
  WebCore::KURL resolve_err(empty_base, rel);
  EXPECT_FALSE(resolve_err.isValid());
}

// WebKit will make empty URLs and set components on them. GURL doesn't allow
// replacements on invalid URLs, but here we do.
TEST(GKURL, ReplaceInvalid) {
  WebCore::KURL gurl;

  EXPECT_FALSE(gurl.isValid());
  EXPECT_TRUE(gurl.isEmpty());
  EXPECT_STREQ("", gurl.string().utf8().data());
  
  gurl.setProtocol("http");
  // GKURL will say that a URL with just a scheme is invalid, KURL will not.
#if USE(GOOGLEURL)
  EXPECT_FALSE(gurl.isValid());
#else
  EXPECT_TRUE(gurl.isValid());
#endif
  EXPECT_FALSE(gurl.isEmpty());
  // At this point, we do things slightly differently if there is only a scheme.
  // We check the results here to make it more obvious what is going on, but it
  // shouldn't be a big deal if these change.
#if USE(GOOGLEURL)
  EXPECT_STREQ("http:", gurl.string().utf8().data());
#else
  EXPECT_STREQ("http:/", gurl.string().utf8().data());
#endif

  gurl.setHost("www.google.com");
  EXPECT_TRUE(gurl.isValid());
  EXPECT_FALSE(gurl.isEmpty());
  EXPECT_STREQ("http://www.google.com/", gurl.string().utf8().data());

  gurl.setPort(8000);
  EXPECT_TRUE(gurl.isValid());
  EXPECT_FALSE(gurl.isEmpty());
  EXPECT_STREQ("http://www.google.com:8000/", gurl.string().utf8().data());

  gurl.setPath("/favicon.ico");
  EXPECT_TRUE(gurl.isValid());
  EXPECT_FALSE(gurl.isEmpty());
  EXPECT_STREQ("http://www.google.com:8000/favicon.ico", gurl.string().utf8().data());

  // Now let's test that giving an invalid replacement still fails.
#if USE(GOOGLEURL)
  gurl.setProtocol("f/sj#@");
  EXPECT_FALSE(gurl.isValid());
#endif
}

TEST(GKURL, Path) {
  const char initial[] = "http://www.google.com/path/foo";
  WebCore::KURL gurl(initial);

  // Clear by setting a NULL string.
  WebCore::String null_string;
  EXPECT_TRUE(null_string.isNull());
  gurl.setPath(null_string);
  EXPECT_STREQ("http://www.google.com/", gurl.string().utf8().data());
}

// Test that setting the query to different things works. Thq query is handled
// a littler differently than some of the other components.
TEST(GKURL, Query) {
  const char initial[] = "http://www.google.com/search?q=awesome";
  WebCore::KURL gurl(initial);

  // Clear by setting a NULL string.
  WebCore::String null_string;
  EXPECT_TRUE(null_string.isNull());
  gurl.setQuery(null_string);
  EXPECT_STREQ("http://www.google.com/search", gurl.string().utf8().data());

  // Clear by setting an empty string.
  gurl = WebCore::KURL(initial);
  WebCore::String empty_string("");
  EXPECT_FALSE(empty_string.isNull());
  gurl.setQuery(empty_string);
  EXPECT_STREQ("http://www.google.com/search?", gurl.string().utf8().data());

  // Set with something that begins in a question mark.
  const char question[] = "?foo=bar";
  gurl.setQuery(question);
  EXPECT_STREQ("http://www.google.com/search?foo=bar",
               gurl.string().utf8().data());

  // Set with something that doesn't begin in a question mark.
  const char query[] = "foo=bar";
  gurl.setQuery(query);
  EXPECT_STREQ("http://www.google.com/search?foo=bar",
               gurl.string().utf8().data());
}

TEST(GKURL, Ref) {
  WebCore::KURL gurl("http://foo/bar#baz");

  // Basic ref setting.
  WebCore::KURL cur("http://foo/bar");
  cur.setRef("asdf");
  EXPECT_STREQ("http://foo/bar#asdf", cur.string().utf8().data());
  cur = gurl;
  cur.setRef("asdf");
  EXPECT_STREQ("http://foo/bar#asdf", cur.string().utf8().data());
  
  // Setting a ref to the empty string will set it to "#".
  cur = WebCore::KURL("http://foo/bar");
  cur.setRef("");
  EXPECT_STREQ("http://foo/bar#", cur.string().utf8().data());
  cur = gurl;
  cur.setRef("");
  EXPECT_STREQ("http://foo/bar#", cur.string().utf8().data());

  // Setting the ref to the null string will clear it altogether.
  cur = WebCore::KURL("http://foo/bar");
  cur.setRef(WebCore::String());
  EXPECT_STREQ("http://foo/bar", cur.string().utf8().data());
  cur = gurl;
  cur.setRef(WebCore::String());
  EXPECT_STREQ("http://foo/bar", cur.string().utf8().data());
}

TEST(GKURL, Empty) {
  WebCore::KURL gurl;

  // First test that regular empty URLs are the same.
  EXPECT_TRUE(gurl.isEmpty());
  EXPECT_FALSE(gurl.isValid());
  EXPECT_TRUE(gurl.isNull());
  EXPECT_TRUE(gurl.string().isNull());
  EXPECT_TRUE(gurl.string().isEmpty());

  // Test resolving a NULL URL on an empty string.
  WebCore::KURL gurl2(gurl, "");
  EXPECT_FALSE(gurl2.isNull());
  EXPECT_TRUE(gurl2.isEmpty());
  EXPECT_FALSE(gurl2.isValid());
  EXPECT_FALSE(gurl2.string().isNull());
  EXPECT_TRUE(gurl2.string().isEmpty());
  EXPECT_FALSE(gurl2.string().isNull());
  EXPECT_TRUE(gurl2.string().isEmpty());

  // Resolve the NULL URL on a NULL string.
  WebCore::KURL gurl22(gurl, WebCore::String());
  EXPECT_FALSE(gurl2.isNull());
  EXPECT_TRUE(gurl2.isEmpty());
  EXPECT_FALSE(gurl2.isValid());
  EXPECT_FALSE(gurl2.string().isNull());
  EXPECT_TRUE(gurl2.string().isEmpty());
  EXPECT_FALSE(gurl2.string().isNull());
  EXPECT_TRUE(gurl2.string().isEmpty());

  // Test non-hierarchical schemes resolving. The actual URLs will be different.
  // WebKit's one will set the string to "something.gif" and we'll set it to an
  // empty string. I think either is OK, so we just check our behavior.
#if USE(GOOGLEURL)
  WebCore::KURL gurl3(WebCore::KURL("data:foo"), "something.gif");
  EXPECT_TRUE(gurl3.isEmpty());
  EXPECT_FALSE(gurl3.isValid());
#endif

  // Test for weird isNull string input,
  // see: http://bugs.webkit.org/show_bug.cgi?id=16487
  WebCore::KURL gurl4(gurl.string());
  EXPECT_TRUE(gurl4.isEmpty());
  EXPECT_FALSE(gurl4.isValid());
  EXPECT_TRUE(gurl4.string().isNull());
  EXPECT_TRUE(gurl4.string().isEmpty());

  // Resolving an empty URL on an invalid string.
  WebCore::KURL gurl5(WebCore::KURL(), "foo.js");
  // We'll be empty in this case, but KURL won't be. Should be OK.
  // EXPECT_EQ(kurl5.isEmpty(), gurl5.isEmpty());
  // EXPECT_EQ(kurl5.string().isEmpty(), gurl5.string().isEmpty());
  EXPECT_FALSE(gurl5.isValid());
  EXPECT_FALSE(gurl5.string().isNull());

  // Empty string as input
  WebCore::KURL gurl6("");
  EXPECT_TRUE(gurl6.isEmpty());
  EXPECT_FALSE(gurl6.isValid());
  EXPECT_FALSE(gurl6.string().isNull());
  EXPECT_TRUE(gurl6.string().isEmpty());

  // Non-empty but invalid C string as input.
  WebCore::KURL gurl7("foo.js");
  // WebKit will actually say this URL has the string "foo.js" but is invalid.
  // We don't do that.
  // EXPECT_EQ(kurl7.isEmpty(), gurl7.isEmpty());
  EXPECT_FALSE(gurl7.isValid());
  EXPECT_FALSE(gurl7.string().isNull());
}

TEST(GKURL, UserPass) {
  const char* src = "http://user:pass@google.com/";
  WebCore::KURL gurl(src);

  // Clear just the username.
  gurl.setUser("");
  EXPECT_EQ("http://:pass@google.com/", gurl.string());

  // Clear just the password.
  gurl = WebCore::KURL(src);
  gurl.setPass("");
  EXPECT_EQ("http://user@google.com/", gurl.string());

  // Now clear both.
  gurl.setUser("");
  EXPECT_EQ("http://google.com/", gurl.string());
}

TEST(GKURL, Offsets) {
  const char* src1 = "http://user:pass@google.com/foo/bar.html?baz=query#ref";
  WebCore::KURL gurl1(src1);

  EXPECT_EQ(17u, gurl1.hostStart());
  EXPECT_EQ(27u, gurl1.hostEnd());
  EXPECT_EQ(27u, gurl1.pathStart());
  EXPECT_EQ(40u, gurl1.pathEnd());
  EXPECT_EQ(32u, gurl1.pathAfterLastSlash());

  const char* src2 = "http://google.com/foo/";
  WebCore::KURL gurl2(src2);

  EXPECT_EQ(7u, gurl2.hostStart());
  EXPECT_EQ(17u, gurl2.hostEnd());
  EXPECT_EQ(17u, gurl2.pathStart());
  EXPECT_EQ(22u, gurl2.pathEnd());
  EXPECT_EQ(22u, gurl2.pathAfterLastSlash());

  const char* src3 = "javascript:foobar";
  WebCore::KURL gurl3(src3);

  EXPECT_EQ(11u, gurl3.hostStart());
  EXPECT_EQ(11u, gurl3.hostEnd());
  EXPECT_EQ(11u, gurl3.pathStart());
  EXPECT_EQ(17u, gurl3.pathEnd());
  EXPECT_EQ(11u, gurl3.pathAfterLastSlash());
}

TEST(GKURL, DeepCopy) {
  const char url[] = "http://www.google.com/";
  WebCore::KURL src(url);
  EXPECT_TRUE(src.string() == url);  // This really just initializes the cache.
  WebCore::KURL dest = src.copy();
  EXPECT_TRUE(dest.string() == url);  // This really just initializes the cache.

  // The pointers should be different for both UTF-8 and UTF-16.
  EXPECT_NE(dest.string().characters(), src.string().characters());
  EXPECT_NE(dest.utf8String().data(), src.utf8String().data());
}
