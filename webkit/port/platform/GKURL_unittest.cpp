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

#include "base/basictypes.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_util.h"

#pragma warning(push, 0)

// This is because we have multiple headers called "CString.h" and KURL.cpp
// can grab the wrong one.
#include "third_party/WebKit/WebCore/platform/text/CString.h"

// This evil preprocessor hack allows us to run both the original KURL and our
// KURL code at the same time regardless of build options.

// Old KURL
#undef USE_GOOGLE_URL_LIBRARY
#define KURL WebKitKURL
#include "KURL.h"
#include "KURL.cpp"

// Google's KURL
#undef KURL_h  // So the header is re-included.
#undef KURL    // Prevent warning on redefinition.
#define USE_GOOGLE_URL_LIBRARY
#define KURL GoogleKURL
#include "KURL.h"
#include "GKURL.cpp"

#pragma warning(pop)

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
                         const WebCore::DeprecatedString& str) {
  return out << WideToUTF8(std::wstring(
    reinterpret_cast<const wchar_t*>(str.unicode()), str.length()));
}

}  // namespace

// Test the cases where we should be the same as WebKit's old KURL
TEST(GKURL, SameGetters) {
  const char* cases[] = {
    // Regular stuff
    "http://www.google.com/foo/blah?bar=baz#ref",
    "http://foo.com:1234/foo/bar/",
    "http://www.google.com?#",
    "https://me:pass@google.com:23#foo",
    "javascript:hello!//world",
  };

  for (int i = 0; i < arraysize(cases); i++) {
    // UTF-8
    WebCore::WebKitKURL kurl(cases[i]);
    WebCore::GoogleKURL gurl(cases[i]);

    EXPECT_EQ(kurl.protocol(), gurl.protocol());
    EXPECT_EQ(kurl.host(), gurl.host());
    EXPECT_EQ(kurl.port(), gurl.port());
    EXPECT_EQ(kurl.user(), gurl.user());
    EXPECT_EQ(kurl.pass(), gurl.pass());
    EXPECT_EQ(kurl.lastPathComponent(), gurl.lastPathComponent());
    EXPECT_EQ(kurl.query(), gurl.query());
    EXPECT_EQ(kurl.ref(), gurl.ref());
    EXPECT_EQ(kurl.hasRef(), gurl.hasRef());

    // UTF-16
    std::wstring wstr(UTF8ToWide(cases[i]));
    WebCore::DeprecatedString utf16(
        reinterpret_cast<const WebCore::DeprecatedChar*>(wstr.c_str()),
        static_cast<int>(wstr.length()));
    kurl = WebCore::WebKitKURL(utf16);
    gurl = WebCore::GoogleKURL(utf16);

    EXPECT_EQ(kurl.protocol(), gurl.protocol());
    EXPECT_EQ(kurl.host(), gurl.host());
    EXPECT_EQ(kurl.port(), gurl.port());
    EXPECT_EQ(kurl.user(), gurl.user());
    EXPECT_EQ(kurl.pass(), gurl.pass());
    EXPECT_EQ(kurl.lastPathComponent(), gurl.lastPathComponent());
    EXPECT_EQ(kurl.query(), gurl.query());
    EXPECT_EQ(kurl.ref(), gurl.ref());
    EXPECT_EQ(kurl.hasRef(), gurl.hasRef());
  };
}

// Test a few cases where we're different just to make sure we give reasonable
// output.
TEST(GKURL, DifferentGetters) {
  ComponentCase cases[] = {
     // url                                   protocol      host        port  user  pass    path                last_path query      ref

    // Old WebKit allows references and queries in what we call "path" URLs
    // like javascript, so the path here will only consist of "hello!".
    {"javascript:hello!?#/\\world",           "javascript", NULL,       0,    NULL, NULL,   "hello!?#/\\world", "world",  NULL,      NULL},

    // Old WebKit doesn't handle "parameters" in paths, so will
    // disagree with us about where the path is for this URL.
    {"http://a.com/hello;world",              "http",       "a.com",    0,    NULL, NULL,   "/hello;world",     "hello",  NULL,      NULL},

    // WebKit doesn't like UTF-8 or UTF-16 input.
    {"http://\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd/", "http", "xn--6qqa088eba", 0, NULL, NULL, "/",   NULL,     NULL,      NULL},

    // WebKit %-escapes non-ASCII characters in reference, but we don't.
    {"http://www.google.com/foo/blah?bar=baz#\xce\xb1\xce\xb2", "http", "www.google.com", 0, NULL, NULL, "/foo/blah/", "blah", "?bar=baz", "\xce\xb1\xce\xb2"}
  };

  for (int i = 0; i < arraysize(cases); i++) {
    WebCore::GoogleKURL gurl(cases[i].url);

    EXPECT_EQ(cases[i].protocol, gurl.protocol());
    EXPECT_EQ(cases[i].host, gurl.host());
    EXPECT_EQ(cases[i].port, gurl.port());
    EXPECT_EQ(cases[i].user, gurl.user());
    EXPECT_EQ(cases[i].pass, gurl.pass());
    EXPECT_EQ(cases[i].last_path, gurl.lastPathComponent());
    EXPECT_EQ(cases[i].query, gurl.query());
    if (cases[i].ref == NULL) {
      EXPECT_EQ(cases[i].ref, gurl.ref());
    } else {
      // We can't compare cases[i].ref and gurl.ref() directly
      // because operator==(const char*, const DeprecatedString&) invoked 
      // in CmpHelperEQ does not treat 'char *' as UTF-8. 
      EXPECT_STREQ(cases[i].ref, gurl.ref().utf8());
    }
  }
}

// Ensures that both ASCII and UTF-8 canonical URLs are handled properly and we
// get the correct string object out.
TEST(GKURL, UTF8) {
  const char ascii_url[] = "http://foo/bar#baz";
  WebCore::GoogleKURL ascii_gurl(ascii_url);
  EXPECT_TRUE(ascii_gurl.string() == WebCore::String(ascii_url));

  // When the result is ASCII, we should get an ASCII DeprecatedString. Some
  // code depends on being able to compare the result of the .deprecatedString()
  // getter with another deprecatedString, and the isASCIIness of the two
  // strings must match for these functions (like equalIgnoringCase).
  EXPECT_TRUE(WebCore::equalIgnoringCase(ascii_gurl.deprecatedString(),
                                         WebCore::DeprecatedString(ascii_url)));

  // Reproduce code path in FrameLoader.cpp -- equalIgnoringCase implicitly
  // expects gkurl.protocol() to have been created as ascii.
  WebCore::GoogleKURL mailto("mailto:foo@foo.com");
  EXPECT_TRUE(WebCore::equalIgnoringCase(mailto.protocol(), "mailto"));

  const char utf8_url[] = "http://foo/bar#\xe4\xbd\xa0\xe5\xa5\xbd";
  WebCore::GoogleKURL utf8_gurl(utf8_url);

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
  // We also can't test clearing the query, since 
  ComponentCase cases[] = {
     // url                                   protocol      host               port  user  pass    path            last_path query      ref
    {"http://www.google.com/",                "https",      "news.google.com", 8888, "me", "pass", "/foo",         NULL,     "?q=asdf", "heehee"},
    {"https://me:pass@google.com:88/a?f#b",   "http",       "goo.com",         92,   "",   "",     "/",            NULL,     NULL,      ""},
  };

  for (int i = 0; i < arraysize(cases); i++) {
    WebCore::GoogleKURL gurl(cases[i].url);
    WebCore::WebKitKURL kurl(cases[i].url);

    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setProtocol(cases[i].protocol);
    gurl.setProtocol(cases[i].protocol);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setHost(cases[i].host);
    gurl.setHost(cases[i].host);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setPort(cases[i].port);
    gurl.setPort(cases[i].port);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setUser(cases[i].user);
    gurl.setUser(cases[i].user);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setPass(cases[i].pass);
    gurl.setPass(cases[i].pass);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setPath(cases[i].path);
    gurl.setPath(cases[i].path);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    kurl.setQuery(cases[i].query);
    gurl.setQuery(cases[i].query);
    EXPECT_EQ(kurl.deprecatedString(), gurl.deprecatedString());

    // Refs are tested below. On the Safari 3.1 branch, we don't match their
    // KURL since we integrated a fix from their trunk.
  }
}

// Tests that KURL::decode_string works as expected
TEST(GKURL, Decode) {
  const char* decode_cases[] = {
    "hello, world",
    "%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/",
    "%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/",
    "%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/",
    "%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/",
    "%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/",
    "%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/",
    "%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/",
    "%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/",
      // Test un-UTF-8-ization.
    "%e4%bd%a0%e5%a5%bd",
  };

  for (int i = 0; i < arraysize(decode_cases); i++) {
    WebCore::DeprecatedString input(decode_cases[i]);
    WebCore::DeprecatedString webkit = WebCore::WebKitKURL::decode_string(input);
    WebCore::DeprecatedString google = WebCore::GoogleKURL::decode_string(input);
    EXPECT_TRUE(webkit == google);
  }

  // Our decode should not decode %00
  WebCore::DeprecatedString zero = WebCore::GoogleKURL::decode_string("%00");
  EXPECT_STREQ("%00", zero.ascii());

  // Test the error behavior for invalid UTF-8 (we differ from WebKit here).
  WebCore::DeprecatedString invalid = WebCore::GoogleKURL::decode_string(
      "%e4%a0%e5%a5%bd");
  WebCore::DeprecatedString invalid_expected(
      reinterpret_cast<const WebCore::DeprecatedChar*>(L"\x00e4\x00a0\x597d"),
      3);
  EXPECT_EQ(invalid_expected, invalid);
}

TEST(GKURL, Encode) {
  // Also test that it gets converted to UTF-8 properly.
  WebCore::DeprecatedString wide_input(
      reinterpret_cast<const WebCore::DeprecatedChar*>(L"\x4f60\x597d"), 2);
  WebCore::DeprecatedString wide_reference("\xe4\xbd\xa0\xe5\xa5\xbd", 6);
  WebCore::DeprecatedString wide_output =
      WebCore::GoogleKURL::encode_string(wide_input);
  EXPECT_EQ(wide_reference, wide_output);

  // Our encode only escapes NULLs for safety (see the implementation for
  // more), so we only bother to test a few cases.
  WebCore::DeprecatedString input(
      "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 16);
  WebCore::DeprecatedString reference(
      "%00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 18);
  WebCore::DeprecatedString output = WebCore::GoogleKURL::encode_string(input);
  EXPECT_EQ(reference, output);
}

TEST(GKURL, ResolveEmpty) {
  WebCore::GoogleKURL empty_base;

  // WebKit likes to be able to resolve absolute input agains empty base URLs,
  // which would normally be invalid since the base URL is invalid.
  const char abs[] = "http://www.google.com/";
  WebCore::GoogleKURL resolve_abs(empty_base, abs);
  EXPECT_TRUE(resolve_abs.isValid());
  EXPECT_STREQ(abs, resolve_abs.deprecatedString().ascii());

  // Resolving a non-relative URL agains the empty one should still error.
  const char rel[] = "foo.html";
  WebCore::GoogleKURL resolve_err(empty_base, rel);
  EXPECT_FALSE(resolve_err.isValid());
}

// WebKit will make empty URLs and set components on them. GURL doesn't allow
// replacements on invalid URLs, but here we do.
TEST(GKURL, ReplaceInvalid) {
  WebCore::GoogleKURL gurl;
  WebCore::WebKitKURL kurl;

  EXPECT_EQ(kurl.isValid(), gurl.isValid());
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());
  
  gurl.setProtocol("http");
  kurl.setProtocol("http");
  // GKURL will say that a URL with just a scheme is invalid, KURL will not.
  EXPECT_TRUE(kurl.isValid());
  EXPECT_FALSE(gurl.isValid());
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  // At this point, the strings will *not* be equal, we do things slightly
  // differently if there is only a scheme. We check the results here to make
  // it more obvious what is going on, but it shouldn't be a big deal if these
  // change.
  EXPECT_STREQ("http:", gurl.deprecatedString().ascii());
  EXPECT_STREQ("http:/", kurl.deprecatedString().ascii());

  gurl.setHost("www.google.com");
  kurl.setHost("www.google.com");
  EXPECT_EQ(kurl.isValid(), gurl.isValid());
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());

  gurl.setPort(8000);
  kurl.setPort(8000);
  EXPECT_EQ(kurl.isValid(), gurl.isValid());
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());

  gurl.setPath("/favicon.ico");
  kurl.setPath("/favicon.ico");
  EXPECT_EQ(kurl.isValid(), gurl.isValid());
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());
  EXPECT_STREQ("http://www.google.com:8000/favicon.ico", gurl.deprecatedString().ascii());

  // Now let's test that giving an invalid replacement still fails.
  gurl.setProtocol("f/sj#@");
  EXPECT_FALSE(gurl.isValid());
}

TEST(GKURL, Path) {
  const char initial[] = "http://www.google.com/path/foo";
  WebCore::GoogleKURL gurl(initial);
  WebCore::WebKitKURL kurl(initial);

  // Clear by setting a NULL string.
  WebCore::DeprecatedString null_string;
  EXPECT_TRUE(null_string.isNull());
  gurl.setPath(null_string);
  kurl.setPath(null_string);
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());
  EXPECT_STREQ("http://www.google.com/", gurl.deprecatedString().ascii());
}

// Test that setting the query to different things works. Thq query is handled
// a littler differently than some of the other components.
TEST(GKURL, Query) {
  const char initial[] = "http://www.google.com/search?q=awesome";
  WebCore::GoogleKURL gurl(initial);
  WebCore::WebKitKURL kurl(initial);

  // Clear by setting a NULL string.
  WebCore::DeprecatedString null_string;
  EXPECT_TRUE(null_string.isNull());
  gurl.setQuery(null_string);
  kurl.setQuery(null_string);
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());

  // Clear by setting an empty string.
  gurl = WebCore::GoogleKURL(initial);
  kurl = WebCore::WebKitKURL(initial);
  WebCore::DeprecatedString empty_string("");
  EXPECT_FALSE(empty_string.isNull());
  gurl.setQuery(empty_string);
  kurl.setQuery(empty_string);
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());

  // Set with something that begins in a question mark.
  const char question[] = "?foo=bar";
  gurl.setQuery(question);
  kurl.setQuery(question);
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());

  // Set with something that doesn't begin in a question mark.
  const char query[] = "foo=bar";
  gurl.setQuery(query);
  kurl.setQuery(query);
  EXPECT_STREQ(kurl.deprecatedString().ascii(), gurl.deprecatedString().ascii());
}

TEST(GKURL, Ref) {
  WebCore::GoogleKURL gurl("http://foo/bar#baz");

  // Basic ref setting.
  WebCore::GoogleKURL cur("http://foo/bar");
  cur.setRef("asdf");
  EXPECT_STREQ(cur.deprecatedString().ascii(), "http://foo/bar#asdf");
  cur = gurl;
  cur.setRef("asdf");
  EXPECT_STREQ(cur.deprecatedString().ascii(), "http://foo/bar#asdf");
  
  // Setting a ref to the empty string will set it to "#".
  cur = WebCore::GoogleKURL("http://foo/bar");
  cur.setRef(WebCore::DeprecatedString(""));
  EXPECT_STREQ("http://foo/bar#", cur.deprecatedString().ascii());
  cur = gurl;
  cur.setRef(WebCore::DeprecatedString(""));
  EXPECT_STREQ("http://foo/bar#", cur.deprecatedString().ascii());

  // Setting the ref to the null string will clear it altogether.
  cur = WebCore::GoogleKURL("http://foo/bar");
  cur.setRef(WebCore::DeprecatedString());
  EXPECT_STREQ("http://foo/bar", cur.deprecatedString().ascii());
  cur = gurl;
  cur.setRef(WebCore::DeprecatedString());
  EXPECT_STREQ("http://foo/bar", cur.deprecatedString().ascii());
}

TEST(GKURL, Empty) {
  WebCore::GoogleKURL gurl;
  WebCore::WebKitKURL kurl;

  // First test that regular empty URLs are the same.
  EXPECT_EQ(kurl.isEmpty(), gurl.isEmpty());
  EXPECT_EQ(kurl.isValid(), gurl.isValid());
  EXPECT_EQ(kurl.string().isNull(), gurl.string().isNull());
  EXPECT_EQ(kurl.string().isEmpty(), gurl.string().isEmpty());

  // Now test that resolved empty URLs are the same.
  WebCore::GoogleKURL gurl2(gurl, "");
  WebCore::WebKitKURL kurl2(kurl, "");
  EXPECT_EQ(kurl2.isEmpty(), gurl2.isEmpty());
  EXPECT_EQ(kurl2.isValid(), gurl2.isValid());
  EXPECT_EQ(kurl2.string().isNull(), gurl2.string().isNull());
  EXPECT_EQ(kurl2.string().isEmpty(), gurl2.string().isEmpty());
  EXPECT_EQ(kurl2.deprecatedString().isNull(), gurl2.deprecatedString().isNull());
  EXPECT_EQ(kurl2.deprecatedString().isEmpty(), gurl2.deprecatedString().isEmpty());

  // Now test that non-hierarchical schemes are resolved the same.
  WebCore::GoogleKURL gurl3(WebCore::GoogleKURL("data:foo"), "something.gif");
  WebCore::WebKitKURL kurl3(WebCore::WebKitKURL("data:foo"), "something.gif");
  EXPECT_EQ(kurl3.isEmpty(), gurl3.isEmpty());
  EXPECT_EQ(kurl3.isValid(), gurl3.isValid());
  EXPECT_EQ(kurl3.string().isNull(), gurl3.string().isNull());
  EXPECT_EQ(kurl3.string().isEmpty(), gurl3.string().isEmpty());

  // Test for weird isNull string input,
  // see: http://bugs.webkit.org/show_bug.cgi?id=16487
  WebCore::GoogleKURL gurl4(gurl.deprecatedString());
  WebCore::WebKitKURL kurl4(kurl.deprecatedString());
  EXPECT_EQ(kurl4.isEmpty(), gurl4.isEmpty());
  EXPECT_EQ(kurl4.isValid(), gurl4.isValid());
  EXPECT_EQ(kurl4.string().isNull(), gurl4.string().isNull());
  EXPECT_EQ(kurl4.string().isEmpty(), gurl4.string().isEmpty());

  // Resolving an empty URL on an invalid string.
  WebCore::GoogleKURL gurl5(WebCore::GoogleKURL(), "foo.js");
  WebCore::WebKitKURL kurl5(WebCore::WebKitKURL(), "foo.js");
  EXPECT_EQ(kurl5.isEmpty(), gurl5.isEmpty());
  EXPECT_EQ(kurl5.isValid(), gurl5.isValid());
  EXPECT_EQ(kurl5.string().isNull(), gurl5.string().isNull());
  EXPECT_EQ(kurl5.string().isEmpty(), gurl5.string().isEmpty());

  // Empty string as input
  WebCore::GoogleKURL gurl6("");
  WebCore::WebKitKURL kurl6("");
  EXPECT_EQ(kurl6.isEmpty(), gurl6.isEmpty());
  EXPECT_EQ(kurl6.isValid(), gurl6.isValid());
  EXPECT_EQ(kurl6.string().isNull(), gurl6.string().isNull());
  EXPECT_EQ(kurl6.string().isEmpty(), gurl6.string().isEmpty());

  // Non-empty but invalid C string as input.
  WebCore::GoogleKURL gurl7("foo.js");
  WebCore::WebKitKURL kurl7("foo.js");
  // WebKit will actually say this URL has the string "foo.js" but is invalid.
  // We don't do that.
  // EXPECT_EQ(kurl7.isEmpty(), gurl7.isEmpty());
  EXPECT_EQ(kurl7.isValid(), gurl7.isValid());
  EXPECT_EQ(kurl7.string().isNull(), gurl7.string().isNull());
}

TEST(GKURL, UserPass) {
  const char* src = "http://user:pass@google.com/";
  WebCore::GoogleKURL gurl(src);
  WebCore::WebKitKURL kurl(src);

  // Clear just the username.
  gurl.setUser("");
  kurl.setUser("");
  EXPECT_TRUE(kurl.string() == gurl.string());

  // Clear just the password.
  gurl = WebCore::GoogleKURL(src);
  kurl = WebCore::WebKitKURL(src);
  gurl.setPass("");
  kurl.setPass("");
  EXPECT_TRUE(kurl.string() == gurl.string());

  // Now clear both.
  gurl.setUser("");
  kurl.setUser("");
  EXPECT_TRUE(kurl.string() == gurl.string());
}
