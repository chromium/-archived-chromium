// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_vary_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test HttpVaryDataTest;

struct TestTransaction {
  net::HttpRequestInfo request;
  scoped_refptr<net::HttpResponseHeaders> response;

  void Init(const std::string& request_headers,
            const std::string& response_headers) {
    std::string temp(response_headers);
    std::replace(temp.begin(), temp.end(), '\n', '\0');
    response = new net::HttpResponseHeaders(temp);

    request.extra_headers = request_headers;
  }
};

}  // namespace

TEST(HttpVaryDataTest, IsInvalid) {
  // All of these responses should result in an invalid vary data object.
  const char* kTestResponses[] = {
    "HTTP/1.1 200 OK\n\n",
    "HTTP/1.1 200 OK\nVary: *\n\n",
    "HTTP/1.1 200 OK\nVary: cookie, *, bar\n\n",
    "HTTP/1.1 200 OK\nVary: cookie\nFoo: 1\nVary: *\n\n",
  };

  for (size_t i = 0; i < arraysize(kTestResponses); ++i) {
    TestTransaction t;
    t.Init("", kTestResponses[i]);

    net::HttpVaryData v;
    EXPECT_FALSE(v.is_valid());
    EXPECT_FALSE(v.Init(t.request, *t.response));
    EXPECT_FALSE(v.is_valid());
  }
}

TEST(HttpVaryDataTest, MultipleInit) {
  net::HttpVaryData v;

  // Init to something valid.
  TestTransaction t1;
  t1.Init("Foo: 1\nbar: 23", "HTTP/1.1 200 OK\nVary: foo, bar\n\n");
  EXPECT_TRUE(v.Init(t1.request, *t1.response));
  EXPECT_TRUE(v.is_valid());

  // Now overwrite by initializing to something invalid.
  TestTransaction t2;
  t2.Init("Foo: 1\nbar: 23", "HTTP/1.1 200 OK\nVary: *\n\n");
  EXPECT_FALSE(v.Init(t2.request, *t2.response));
  EXPECT_FALSE(v.is_valid());
}

TEST(HttpVaryDataTest, DoesVary) {
  TestTransaction a;
  a.Init("Foo: 1", "HTTP/1.1 200 OK\nVary: foo\n\n");

  TestTransaction b;
  b.Init("Foo: 2", "HTTP/1.1 200 OK\nVary: foo\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response));
}

TEST(HttpVaryDataTest, DoesVary2) {
  TestTransaction a;
  a.Init("Foo: 1\nbar: 23", "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  TestTransaction b;
  b.Init("Foo: 12\nbar: 3", "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response));
}

TEST(HttpVaryDataTest, DoesntVary) {
  TestTransaction a;
  a.Init("Foo: 1", "HTTP/1.1 200 OK\nVary: foo\n\n");

  TestTransaction b;
  b.Init("Foo: 1", "HTTP/1.1 200 OK\nVary: foo\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_TRUE(v.MatchesRequest(b.request, *b.response));
}

TEST(HttpVaryDataTest, DoesntVary2) {
  TestTransaction a;
  a.Init("Foo: 1\nbAr: 2", "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  TestTransaction b;
  b.Init("Foo: 1\nbaR: 2", "HTTP/1.1 200 OK\nVary: foo\nVary: bar\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_TRUE(v.MatchesRequest(b.request, *b.response));
}

TEST(HttpVaryDataTest, ImplicitCookieForRedirect) {
  TestTransaction a;
  a.Init("Cookie: 1", "HTTP/1.1 301 Moved\nLocation: x\n\n");

  TestTransaction b;
  b.Init("Cookie: 2", "HTTP/1.1 301 Moved\nLocation: x\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response));
}

TEST(HttpVaryDataTest, ImplicitCookieForRedirect2) {
  // This should be no different than the test above

  TestTransaction a;
  a.Init("Cookie: 1", "HTTP/1.1 301 Moved\nLocation: x\nVary: coOkie\n\n");

  TestTransaction b;
  b.Init("Cookie: 2", "HTTP/1.1 301 Moved\nLocation: x\nVary: cooKie\n\n");

  net::HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response));
}
