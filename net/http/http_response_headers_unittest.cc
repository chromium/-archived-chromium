// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#include <algorithm>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/time.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace std;
using net::HttpResponseHeaders;

namespace {

struct TestData {
  const char* raw_headers;
  const char* expected_headers;
  int expected_response_code;
};

struct ContentTypeTestData {
  const string raw_headers;
  const string mime_type;
  const bool has_mimetype;
  const string charset;
  const bool has_charset;
  const string all_content_type;
};

class HttpResponseHeadersTest : public testing::Test {
};

// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

void TestCommon(const TestData& test) {
  string raw_headers(test.raw_headers);
  HeadersToRaw(&raw_headers);
  string expected_headers(test.expected_headers);

  string headers;
  scoped_refptr<HttpResponseHeaders> parsed =
      new HttpResponseHeaders(raw_headers);
  parsed->GetNormalizedHeaders(&headers);

  // Transform to readable output format (so it's easier to see diffs).
  replace(headers.begin(), headers.end(), ' ', '_');
  replace(headers.begin(), headers.end(), '\n', '\\');
  replace(expected_headers.begin(), expected_headers.end(), ' ', '_');
  replace(expected_headers.begin(), expected_headers.end(), '\n', '\\');

  EXPECT_EQ(expected_headers, headers);

  EXPECT_EQ(test.expected_response_code, parsed->response_code());
}

} // end namespace

// Check that we normalize headers properly.
TEST(HttpResponseHeadersTest, NormalizeHeadersWhitespace) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "  Content-TYPE  : text/html; charset=utf-8  \n"
    "Set-Cookie: a \n"
    "Set-Cookie:   b \n",

    "HTTP/1.1 202 Accepted\n"
    "Content-TYPE: text/html; charset=utf-8\n"
    "Set-Cookie: a, b\n",

    202
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, BlankHeaders) {
  TestData test = {
    "HTTP/1.1 200 OK\n"
    "Header1 :          \n"
    "Header2: \n"
    "Header3:\n"
    "Header4\n"
    "Header5    :\n",

    "HTTP/1.1 200 OK\n"
    "Header1: \n"
    "Header2: \n"
    "Header3: \n"
    "Header5: \n",

    200
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersVersion) {
  TestData test = {
    "hTtP/0.9 201\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.0 201 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    201
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersMissingOK) {
  TestData test = {
    "HTTP/1.1 201\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.1 201 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    201
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersBadStatus) {
  TestData test = {
    "SCREWED_UP_STATUS_LINE\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    "HTTP/1.0 200 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n",

    200
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersEmpty) {
  TestData test = {
    "",

    "HTTP/1.0 200 OK\n",

    200
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersStartWithColon) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "foo: bar\n"
    ": a \n"
    " : b\n"
    "baz: blat \n",

    "HTTP/1.1 202 Accepted\n"
    "foo: bar\n"
    "baz: blat\n",

    202
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersStartWithColonAtEOL) {
  TestData test = {
    "HTTP/1.1    202   Accepted  \n"
    "foo:   \n"
    "bar:\n"
    "baz: blat \n"
    "zip:\n",

    "HTTP/1.1 202 Accepted\n"
    "foo: \n"
    "bar: \n"
    "baz: blat\n"
    "zip: \n",

    202
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, NormalizeHeadersOfWhitepace) {
  TestData test = {
    "\n   \n",

    "HTTP/1.0 200 OK\n",

    200
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, RepeatedSetCookie) {
  TestData test = {
    "HTTP/1.1 200 OK\n"
    "Set-Cookie: x=1\n"
    "Set-Cookie: y=2\n",

    "HTTP/1.1 200 OK\n"
    "Set-Cookie: x=1, y=2\n",

    200
  };
  TestCommon(test);
}

TEST(HttpResponseHeadersTest, GetNormalizedHeader) {
  std::string headers =
    "HTTP/1.1 200 OK\n"
    "Cache-control: private\n"
    "cache-Control: no-store\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed = new HttpResponseHeaders(headers);

  std::string value;
  EXPECT_TRUE(parsed->GetNormalizedHeader("cache-control", &value));
  EXPECT_EQ("private, no-store", value);
}

TEST(HttpResponseHeadersTest, Persist) {
  const struct {
    const char* raw_headers;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Cache-control:private\n"
      "cache-Control:no-store\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: private, no-store\n"
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "server: blah\n",

      "HTTP/1.1 200 OK\n"
      "server: blah\n"
    },
    { "HTTP/1.1 200 OK\n"
      "fOo: 1\n"
      "Foo: 2\n"
      "Transfer-Encoding: chunked\n"
      "CoNnection: keep-alive\n"
      "cache-control: private, no-cache=\"foo\"\n",

      "HTTP/1.1 200 OK\n"
      "cache-control: private, no-cache=\"foo\"\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=\"foo, bar\"\n"
      "bar",

      "HTTP/1.1 200 OK\n"
      "Cache-Control: private,no-cache=\"foo, bar\"\n"
    },
    // ignore bogus no-cache value
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=foo\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private,no-cache=foo\n"
    },
    // ignore bogus no-cache value
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\n"
    },
    // ignore empty no-cache value
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"\"\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"\"\n"
    },
    // ignore wrong quotes no-cache value
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\'foo\'\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\'foo\'\n"
    },
    // ignore unterminated quotes no-cache value
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"foo\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\"foo\n"
    },
    // accept sloppy LWS
    { "HTTP/1.1 200 OK\n"
      "Foo: 2\n"
      "Cache-Control: private, no-cache=\" foo\t, bar\"\n",

      "HTTP/1.1 200 OK\n"
      "Cache-Control: private, no-cache=\" foo\t, bar\"\n"
    },
    // header name appears twice, separated by another header
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n"
      "Foo: 3\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 1, 3\n"
      "Bar: 2\n"
    },
    // header name appears twice, separated by another header (type 2)
    { "HTTP/1.1 200 OK\n"
      "Foo: 1, 3\n"
      "Bar: 2\n"
      "Foo: 4\n",

      "HTTP/1.1 200 OK\n"
      "Foo: 1, 3, 4\n"
      "Bar: 2\n"
    },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers = tests[i].raw_headers;
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed1 =
        new HttpResponseHeaders(headers);

    Pickle pickle;
    parsed1->Persist(&pickle, true);

    void* iter = NULL;
    scoped_refptr<HttpResponseHeaders> parsed2 =
        new HttpResponseHeaders(pickle, &iter);

    std::string h2;
    parsed2->GetNormalizedHeaders(&h2);
    EXPECT_EQ(string(tests[i].expected_headers), h2);
  }
}

TEST(HttpResponseHeadersTest, EnumerateHeader_Coalesced) {
  // Ensure that commas in quoted strings are not regarded as value separators.
  // Ensure that whitespace following a value is trimmed properly
  std::string headers =
    "HTTP/1.1 200 OK\n"
    "Cache-control:private , no-cache=\"set-cookie,server\" \n"
    "cache-Control: no-store\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed = new HttpResponseHeaders(headers);

  void* iter = NULL;
  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("private", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-cache=\"set-cookie,server\"", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-store", value);
}

TEST(HttpResponseHeadersTest, EnumerateHeader_DateValued) {
  // The comma in a date valued header should not be treated as a
  // field-value separator
  std::string headers =
    "HTTP/1.1 200 OK\n"
    "Date: Tue, 07 Aug 2007 23:10:55 GMT\n"
    "Last-Modified: Wed, 01 Aug 2007 23:23:45 GMT\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed = new HttpResponseHeaders(headers);

  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(NULL, "date", &value));
  EXPECT_EQ("Tue, 07 Aug 2007 23:10:55 GMT", value);
  EXPECT_TRUE(parsed->EnumerateHeader(NULL, "last-modified", &value));
  EXPECT_EQ("Wed, 01 Aug 2007 23:23:45 GMT", value);
}

TEST(HttpResponseHeadersTest, GetMimeType) {
  const ContentTypeTestData tests[] = {
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/html" },
    // Multiple content-type headers should give us the last one.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n"
        "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/html, text/html" },
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/plain\n"
        "Content-type: text/html\n"
        "Content-type: text/plain\n"
        "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/plain, text/html, text/plain, text/html" },
    // Test charset parsing.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n"
        "Content-type: text/html; charset=ISO-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html, text/html; charset=ISO-8859-1" },
    // Test charset in double quotes.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n"
        "Content-type: text/html; charset=\"ISO-8859-1\"\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html, text/html; charset=\"ISO-8859-1\"" },
    // If there are multiple matching content-type headers, we carry
    // over the charset value.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html;charset=utf-8\n"
        "Content-type: text/html\n",
      "text/html", true,
      "utf-8", true,
      "text/html;charset=utf-8, text/html" },
    // Test single quotes.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html;charset='utf-8'\n"
        "Content-type: text/html\n",
      "text/html", true,
      "utf-8", true,
      "text/html;charset='utf-8', text/html" },
    // Last charset wins if matching content-type.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html;charset=utf-8\n"
        "Content-type: text/html;charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html;charset=utf-8, text/html;charset=iso-8859-1" },
    // Charset is ignored if the content types change.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/plain;charset=utf-8\n"
        "Content-type: text/html\n",
      "text/html", true,
      "", false,
      "text/plain;charset=utf-8, text/html" },
    // Empty content-type
    { "HTTP/1.1 200 OK\n"
        "Content-type: \n",
      "", false,
      "", false,
      "" },
    // Emtpy charset
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html;charset=\n",
      "text/html", true,
      "", false,
      "text/html;charset=" },
    // Multiple charsets, last one wins.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html;charset=utf-8; charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html;charset=utf-8; charset=iso-8859-1" },
    // Multiple params.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html; foo=utf-8; charset=iso-8859-1\n",
      "text/html", true,
      "iso-8859-1", true,
      "text/html; foo=utf-8; charset=iso-8859-1" },
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html ; charset=utf-8 ; bar=iso-8859-1\n",
      "text/html", true,
      "utf-8", true,
      "text/html ; charset=utf-8 ; bar=iso-8859-1" },
    // Comma embeded in quotes.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html ; charset='utf-8,text/plain' ;\n",
      "text/html", true,
      "utf-8,text/plain", true,
      "text/html ; charset='utf-8,text/plain' ;" },
    // Charset with leading spaces.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html ; charset= 'utf-8' ;\n",
      "text/html", true,
      "utf-8", true,
      "text/html ; charset= 'utf-8' ;" },
    // Media type comments in mime-type.
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html (html)\n",
      "text/html", true,
      "", false,
      "text/html (html)" },
    // Incomplete charset= param
    { "HTTP/1.1 200 OK\n"
        "Content-type: text/html; char=\n",
      "text/html", true,
      "", false,
      "text/html; char=" },
    // Invalid media type: no slash
    { "HTTP/1.1 200 OK\n"
        "Content-type: texthtml\n",
      "", false,
      "", false,
      "texthtml" },
    // Invalid media type: */*
    { "HTTP/1.1 200 OK\n"
        "Content-type: */*\n",
      "", false,
      "", false,
      "*/*" },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].raw_headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed = new HttpResponseHeaders(headers);

    std::string value;
    EXPECT_EQ(tests[i].has_mimetype, parsed->GetMimeType(&value));
    EXPECT_EQ(tests[i].mime_type, value);
    value.clear();
    EXPECT_EQ(tests[i].has_charset, parsed->GetCharset(&value));
    EXPECT_EQ(tests[i].charset, value);
    EXPECT_TRUE(parsed->GetNormalizedHeader("content-type", &value));
    EXPECT_EQ(tests[i].all_content_type, value);
  }
}

TEST(HttpResponseHeadersTest, RequiresValidation) {
  const struct {
    const char* headers;
    bool requires_validation;
  } tests[] = {
    // no expiry info: expires immediately
    { "HTTP/1.1 200 OK\n"
      "\n",
      true
    },
    // valid for a little while
    { "HTTP/1.1 200 OK\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // expires in the future
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 01:00:00 GMT\n"
      "\n",
      false
    },
    // expired already
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
      "\n",
      true
    },
    // max-age trumps expires
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // last-modified heuristic: modified a while ago
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "\n",
      false
    },
    // last-modified heuristic: modified recently
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
      "\n",
      true
    },
    // cached permanent redirect
    { "HTTP/1.1 301 Moved Permanently\n"
      "\n",
      false
    },
    // cached redirect: not reusable even though by default it would be
    { "HTTP/1.1 300 Multiple Choices\n"
      "Cache-Control: no-cache\n"
      "\n",
      true
    },
    // cached forever by default
    { "HTTP/1.1 410 Gone\n"
      "\n",
      false
    },
    // cached temporary redirect: not reusable
    { "HTTP/1.1 302 Found\n"
      "\n",
      true
    },
    // cached temporary redirect: reusable
    { "HTTP/1.1 302 Found\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // cache-control: max-age=N overrides expires: date in the past
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 28 Nov 2007 00:20:11 GMT\n"
      "cache-control: max-age=10000\n"
      "\n",
      false
    },
    // cache-control: no-store overrides expires: in the future
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "expires: Wed, 29 Nov 2007 00:40:11 GMT\n"
      "cache-control: no-store,private,no-cache=\"foo\"\n"
      "\n",
      true
    },
    // pragma: no-cache overrides last-modified heuristic
    { "HTTP/1.1 200 OK\n"
      "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
      "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
      "pragma: no-cache\n"
      "\n",
      true
    },
    // TODO(darin): add many many more tests here
  };
  Time request_time, response_time, current_time;
  Time::FromString(L"Wed, 28 Nov 2007 00:40:09 GMT", &request_time);
  Time::FromString(L"Wed, 28 Nov 2007 00:40:12 GMT", &response_time);
  Time::FromString(L"Wed, 28 Nov 2007 00:45:20 GMT", &current_time);

  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed = new HttpResponseHeaders(headers);

    bool requires_validation =
        parsed->RequiresValidation(request_time, response_time, current_time);
    EXPECT_EQ(tests[i].requires_validation, requires_validation);
  }
}

TEST(HttpResponseHeadersTest, Update) {
  const struct {
    const char* orig_headers;
    const char* new_headers;
    const char* expected_headers;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: max-age=10000\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Cache-control: private\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-control: max-age=10000\n"
      "Foo: 1\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Cache-control: private\n",

      "HTTP/1/1 304 Not Modified\n"
      "connection: keep-alive\n"
      "Cache-CONTROL: max-age=10000\n",

      "HTTP/1.1 200 OK\n"
      "Cache-CONTROL: max-age=10000\n"
      "Foo: 1\n"
    },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    string orig_headers(tests[i].orig_headers);
    HeadersToRaw(&orig_headers);
    scoped_refptr<HttpResponseHeaders> parsed =
        new HttpResponseHeaders(orig_headers);

    string new_headers(tests[i].new_headers);
    HeadersToRaw(&new_headers);
    scoped_refptr<HttpResponseHeaders> new_parsed =
        new HttpResponseHeaders(new_headers);

    parsed->Update(*new_parsed);

    string resulting_headers;
    parsed->GetNormalizedHeaders(&resulting_headers);
    EXPECT_EQ(string(tests[i].expected_headers), resulting_headers);
  }
}

TEST(HttpResponseHeadersTest, EnumerateHeaderLines) {
  const struct {
    const char* headers;
    const char* expected_lines;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",

      ""
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n",

      "Foo: 1\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n"
      "Foo: 3\n",

      "Foo: 1\nBar: 2\nFoo: 3\n"
    },
    { "HTTP/1.1 200 OK\n"
      "Foo: 1, 2, 3\n",

      "Foo: 1, 2, 3\n"
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed =
        new HttpResponseHeaders(headers);

    string name, value, lines;

    void* iter = NULL;
    while (parsed->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }

    EXPECT_EQ(string(tests[i].expected_lines), lines);
  }
}

TEST(HttpResponseHeadersTest, IsRedirect) {
  const struct {
    const char* headers;
    const char* location;
    bool is_redirect;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",
      "",
      false
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foopy/\n",
      "http://foopy/",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: \t \n",
      "",
      false
    },
    // we use the first location header as the target of the redirect
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/\n"
      "Location: http://bar/\n",
      "http://foo/",
      true
    },
    // we use the first _valid_ location header as the target of the redirect
    { "HTTP/1.1 301 Moved\n"
      "Location: \n"
      "Location: http://bar/\n",
      "http://bar/",
      true
    },
    // bug 1050541 (location header w/ an unescaped comma)
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar,baz.html\n",
      "http://foo/bar,baz.html",
      true
    },
    // bug 1224617 (location header w/ non-ASCII bytes)
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\xE4\xF6\xFC\n",
      "http://foo/bar?key=%E4%F6%FC",
      true
    },
    // Shift_JIS, Big5, and GBK contain multibyte characters with the trailing
    // byte falling in the ASCII range.
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x81\x5E\xD8\xBF\n",
      "http://foo/bar?key=%81^%D8%BF",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x82\x40\xBD\xC4\n",
      "http://foo/bar?key=%82@%BD%C4",
      true
    },
    { "HTTP/1.1 301 Moved\n"
      "Location: http://foo/bar?key=\x83\x5C\x82\x5D\xCB\xD7\n",
      "http://foo/bar?key=%83\\%82]%CB%D7",
      true
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed =
        new HttpResponseHeaders(headers);

    std::string location;
    EXPECT_EQ(parsed->IsRedirect(&location), tests[i].is_redirect);
    EXPECT_EQ(location, tests[i].location);
  }
}

TEST(HttpResponseHeadersTest, GetContentLength) {
  const struct {
    const char* headers;
    int64 expected_len;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: abc\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: -10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length:  +10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 23xb5\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 0xA\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 010\n",
      10
    },
    // Content-Length too big, will overflow an int64
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 40000000000000000000\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length:       10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 10  \n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \t10\n",
      10
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \v10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: \f10\n",
      -1
    },
    { "HTTP/1.1 200 OK\n"
      "cOnTeNt-LENgth: 33\n",
      33
    },
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 34\r\n",
      -1
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed =
        new HttpResponseHeaders(headers);

    EXPECT_EQ(tests[i].expected_len, parsed->GetContentLength());
  }
}

TEST(HttpResponseHeadersTest, IsKeepAlive) {
  const struct {
    const char* headers;
    bool expected_keep_alive;
  } tests[] = {
    { "HTTP/1.1 200 OK\n",
      true
    },
    { "HTTP/1.0 200 OK\n",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "connection: close\n",
      false
    },
    { "HTTP/1.0 200 OK\n"
      "connection: keep-alive\n",
      true
    },
    { "HTTP/1.0 200 OK\n"
      "connection: kEeP-AliVe\n",
      true
    },
    { "HTTP/1.0 200 OK\n"
      "connection: keep-aliveX\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "connection: close\n",
      false
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n",
      true
    },
    { "HTTP/1.1 200 OK\n"
      "proxy-connection: close\n",
      false
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> parsed =
        new HttpResponseHeaders(headers);

    EXPECT_EQ(tests[i].expected_keep_alive, parsed->IsKeepAlive());
  }
}
