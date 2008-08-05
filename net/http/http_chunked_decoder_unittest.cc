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

#include "base/basictypes.h"
#include "net/http/http_chunked_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test HttpChunkedDecoderTest;

void RunTest(const char* inputs[], size_t num_inputs,
             const char* expected_output,
             bool expected_eof) {
  net::HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  std::string result;

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    EXPECT_GE(n, 0);
    if (n > 0)
      result.append(input.data(), n);
  }

  EXPECT_TRUE(result == expected_output);
  EXPECT_TRUE(decoder.reached_eof() == expected_eof);
}

}  // namespace

TEST(HttpChunkedDecoderTest, Basic) {
  const char* inputs[] = {
    "5\r\nhello\r\n0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, OneChunk) {
  const char* inputs[] = {
    "5\r\nhello\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", false);
}

TEST(HttpChunkedDecoderTest, Typical) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "1\r\n \r\n",
    "5\r\nworld\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello world", true);
}

TEST(HttpChunkedDecoderTest, Incremental) {
  const char* inputs[] = {
    "5",
    "\r",
    "\n",
    "hello",
    "\r",
    "\n",
    "0",
    "\r",
    "\n",
    "\r",
    "\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, Extensions) {
  const char* inputs[] = {
    "5;x=0\r\nhello\r\n",
    "0;y=\"2 \"\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, Trailers) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n",
    "Bar: 2\r\n",
    "\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}
