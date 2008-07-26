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

TEST(HttpVaryDataTest, IsValid) {
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
    EXPECT_FALSE(v.Init(t.request, *t.response));
    EXPECT_FALSE(v.is_valid());
  }
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
