// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/perftimer.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_stdstring.h"
#include "googleurl/src/url_parse.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(darin): chrome code should not depend on WebCore innards
#if 0
#pragma warning(push, 0)

// This is because we have multiple headers called "CString.h" and KURL.cpp
// can grab the wrong one.
#include "webkit/third_party/WebCore/platform/CString.h"

#undef USE_GOOGLE_URL_LIBRARY
#define KURL WebKitKURL
#include "KURL.h"
#include "KURL.cpp"
#pragma warning(pop)

TEST(URLParse, FullURL) {
  const char url[] = "http://me:pass@host/foo/bar.html;param?query=yes#ref";
  int url_len = static_cast<int>(strlen(url));

  url_parse::Parsed parsed;
  PerfTimeLogger timer("Full_URL_Parse_AMillion");

  for (int i = 0; i < 1000000; i++)
    url_parse::ParseStandardURL(url, url_len, &parsed);
  timer.Done();
}

namespace {

const char typical_url1[] = "http://www.google.com/search?q=url+parsing&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:official&client=firefox-a";
int typical_url1_len = static_cast<int>(strlen(typical_url1));

const char typical_url2[] = "http://www.amazon.com/Stephen-King-Thrillers-Horror-People/dp/0766012336/ref=sr_1_2/133-4144931-4505264?ie=UTF8&s=books&qid=2144880915&sr=8-2";
int typical_url2_len = static_cast<int>(strlen(typical_url2));

const char typical_url3[] = "http://store.apple.com/1-800-MY-APPLE/WebObjects/AppleStore.woa/wa/RSLID?nnmm=browse&mco=578E9744&node=home/desktop/mac_pro";
int typical_url3_len = static_cast<int>(strlen(typical_url3));

}

TEST(URLParse, TypicalURLParse) {
  url_parse::Parsed parsed1;
  url_parse::Parsed parsed2;
  url_parse::Parsed parsed3;

  // Do this 1/3 of a million times since we do 3 different URLs.
  PerfTimeLogger parse_timer("Typical_URL_Parse_AMillion");
  for (int i = 0; i < 333333; i++) {
    url_parse::ParseStandardURL(typical_url1, typical_url1_len, &parsed1);
    url_parse::ParseStandardURL(typical_url2, typical_url2_len, &parsed2);
    url_parse::ParseStandardURL(typical_url3, typical_url3_len, &parsed3);
  }
  parse_timer.Done();
}

// Includes both parsing and canonicalization with no mallocs.
TEST(URLParse, TypicalURLParseCanon) {
  url_parse::Parsed parsed1;
  url_parse::Parsed parsed2;
  url_parse::Parsed parsed3;

  PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url_parse::Parsed out_parsed;
  url_canon::RawCanonOutput<1024> output;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    url_parse::ParseStandardURL(typical_url1, typical_url1_len, &parsed1);
    output.set_length(0);
    url_canon::CanonicalizeStandardURL(typical_url1, typical_url1_len, parsed1,
                                       NULL, &output, &out_parsed);

    url_parse::ParseStandardURL(typical_url2, typical_url2_len, &parsed2);
    output.set_length(0);
    url_canon::CanonicalizeStandardURL(typical_url2, typical_url2_len, parsed2,
                                       NULL, &output, &out_parsed);

    url_parse::ParseStandardURL(typical_url3, typical_url3_len, &parsed3);
    output.set_length(0);
    url_canon::CanonicalizeStandardURL(typical_url3, typical_url3_len, parsed3,
                                       NULL, &output, &out_parsed);
  }
  canon_timer.Done();
}

// Includes both parsing and canonicalization, and mallocs for the output.
TEST(URLParse, TypicalURLParseCanonStdString) {
  url_parse::Parsed parsed1;
  url_parse::Parsed parsed2;
  url_parse::Parsed parsed3;

  PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url_parse::Parsed out_parsed;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    url_parse::ParseStandardURL(typical_url1, typical_url1_len, &parsed1);
    std::string out1;
    url_canon::StdStringCanonOutput output1(&out1);
    url_canon::CanonicalizeStandardURL(typical_url1, typical_url1_len, parsed1,
                                       NULL, &output1, &out_parsed);

    url_parse::ParseStandardURL(typical_url2, typical_url2_len, &parsed2);
    std::string out2;
    url_canon::StdStringCanonOutput output2(&out2);
    url_canon::CanonicalizeStandardURL(typical_url2, typical_url2_len, parsed2,
                                       NULL, &output2, &out_parsed);

    url_parse::ParseStandardURL(typical_url3, typical_url3_len, &parsed3);
    std::string out3;
    url_canon::StdStringCanonOutput output3(&out3);
    url_canon::CanonicalizeStandardURL(typical_url3, typical_url3_len, parsed3,
                                       NULL, &output3, &out_parsed);
  }
  canon_timer.Done();
}

TEST(URLParse, GURL) {
  // Don't want to time creating the input strings.
  std::string stdurl1(typical_url1);
  std::string stdurl2(typical_url2);
  std::string stdurl3(typical_url3);

  PerfTimeLogger gurl_timer("Typical_GURL_AMillion");
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    GURL gurl1(stdurl1);
    GURL gurl2(stdurl2);
    GURL gurl3(stdurl3);
  }
  gurl_timer.Done();
}

// TODO(darin): chrome code should not depend on WebCore innards
TEST(URLParse, KURL) {
  PerfTimeLogger timer_kurl("Typical_KURL_AMillion");
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    WebCore::WebKitKURL kurl1(typical_url1);
    WebCore::WebKitKURL kurl2(typical_url2);
    WebCore::WebKitKURL kurl3(typical_url3);
  }
  timer_kurl.Done();
}

#endif
