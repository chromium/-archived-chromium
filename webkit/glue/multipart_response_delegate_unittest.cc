// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "KURL.h"
#include "ResourceResponse.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace WebCore;
using namespace std;

namespace {

class MultipartResponseTest : public testing::Test {
};

class MockResourceHandleClient : public ResourceHandleClient {
 public:
  MockResourceHandleClient() { Reset(); }

  virtual void didReceiveResponse(ResourceHandle* handle,
                                  const ResourceResponse& response) {
    ++received_response_;
    resource_response_ = response;
    data_.clear();
  }
  virtual void didReceiveData(ResourceHandle* handle,
                              const char* data, int data_length,
                              int length_received) {
    ++received_data_;
    data_.append(data, data_length);
  }

  void Reset() {
    received_response_ = received_data_ = 0;
    data_.clear();
    resource_response_ = ResourceResponse();
  }

  int received_response_, received_data_;
  string data_;
  ResourceResponse resource_response_;
};

}  // namespace

// We can't put this in an anonymous function because it's a friend class for
// access to private members.
TEST(MultipartResponseTest, Functions) {
  // PushOverLine tests

  ResourceResponse response(KURL(), "multipart/x-mixed-replace", 0, "en-US",
                            String());
  response.setHTTPHeaderField(String("Foo"), String("Bar"));
  response.setHTTPHeaderField(String("Content-type"), String("text/plain"));
  MockResourceHandleClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  struct {
    const char* input;
    const int position;
    const int expected;
  } line_tests[] = {
    { "Line", 0, 0 },
    { "Line", 2, 0 },
    { "Line", 10, 0 },
    { "\r\nLine", 0, 2 },
    { "\nLine", 0, 1 },
    { "\n\nLine", 0, 2 },
    { "\rLine", 0, 1 },
    { "Line\r\nLine", 4, 2 },
    { "Line\nLine", 4, 1 },
    { "Line\n\nLine", 4, 2 },
    { "Line\rLine", 4, 1 },
    { "Line\r\rLine", 4, 1 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(line_tests); ++i) {
    EXPECT_EQ(line_tests[i].expected,
              delegate.PushOverLine(line_tests[i].input,
                                    line_tests[i].position));
  }

  // ParseHeaders tests
  struct {
    const char* data;
    const bool rv;
    const int received_response_calls;
    const char* newdata;
  } header_tests[] = {
    { "This is junk", false, 0, "This is junk" },
    { "Foo: bar\nBaz:\n\nAfter:\n", true, 1, "After:\n" },
    { "Foo: bar\nBaz:\n", false, 0, "Foo: bar\nBaz:\n" },
    { "Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 1, "After:\r\n" },
    { "Foo: bar\r\nBaz:\r\n", false, 0, "Foo: bar\r\nBaz:\r\n" },
    { "Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 1, "After:\n\n" },
    { "Foo: bar\r\nBaz:\n", false, 0, "Foo: bar\r\nBaz:\n" },
    { "\r\n", true, 1, "" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(header_tests); ++i) {
    client.Reset();
    delegate.data_.assign(header_tests[i].data);
    EXPECT_EQ(header_tests[i].rv,
              delegate.ParseHeaders());
    EXPECT_EQ(header_tests[i].received_response_calls,
              client.received_response_);
    EXPECT_EQ(string(header_tests[i].newdata),
              delegate.data_);
  }
  // Test that the resource response is filled in correctly when parsing
  // headers.
  client.Reset();
  string test_header("content-type: image/png\ncontent-length: 10\n\n");
  delegate.data_.assign(test_header);
  EXPECT_TRUE(delegate.ParseHeaders());
  EXPECT_TRUE(delegate.data_.length() == 0);
  EXPECT_EQ(webkit_glue::StringToStdWString(
              client.resource_response_.httpHeaderField(
                String("Content-Type"))),
            wstring(L"image/png"));
  EXPECT_EQ(webkit_glue::StringToStdWString(
              client.resource_response_.httpHeaderField(
                String("content-length"))),
            wstring(L"10"));
  // This header is passed from the original request.
  EXPECT_EQ(webkit_glue::StringToStdWString(
              client.resource_response_.httpHeaderField(String("foo"))),
            wstring(L"Bar"));

  // FindBoundary tests
  struct {
    const char* boundary;
    const char* data;
    const size_t position;
  } boundary_tests[] = {
    { "bound", "bound", 0 },
    { "bound", "--bound", 0 },
    { "bound", "junkbound", 4 },
    { "bound", "junk--bound", 4 },
    { "foo", "bound", string::npos },
    { "bound", "--boundbound", 0 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(boundary_tests); ++i) {
    delegate.boundary_.assign(boundary_tests[i].boundary);
    delegate.data_.assign(boundary_tests[i].data);
    EXPECT_EQ(boundary_tests[i].position,
              delegate.FindBoundary());
  }
}

namespace {

TEST(MultipartResponseTest, MissingBoundaries) {
  ResourceResponse response(KURL(), "multipart/x-mixed-replace", 0, "en-US",
                            String());
  response.setHTTPHeaderField(String("Foo"), String("Bar"));
  response.setHTTPHeaderField(String("Content-type"), String("text/plain"));
  MockResourceHandleClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  // No start boundary
  string no_start_boundary(
    "Content-type: text/plain\n\n"
    "This is a sample response\n"
    "--bound--"
    "ignore junk after end token --bound\n\nTest2\n");
  delegate.OnReceivedData(no_start_boundary.c_str(),
                          static_cast<int>(no_start_boundary.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"),
            client.data_);

  delegate.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);

  // No end boundary
  client.Reset();
  MultipartResponseDelegate delegate2(&client, NULL, response, "bound");
  string no_end_boundary(
    "bound\nContent-type: text/plain\n\n"
    "This is a sample response\n");
  delegate2.OnReceivedData(no_end_boundary.c_str(),
                          static_cast<int>(no_end_boundary.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(0, client.received_data_);
  EXPECT_EQ(string(), client.data_);

  delegate2.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"),
            client.data_);

  // Neither boundary
  client.Reset();
  MultipartResponseDelegate delegate3(&client, NULL, response, "bound");
  string no_boundaries(
    "Content-type: text/plain\n\n"
    "This is a sample response\n");
  delegate3.OnReceivedData(no_boundaries.c_str(),
                          static_cast<int>(no_boundaries.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(0, client.received_data_);
  EXPECT_EQ(string(), client.data_);

  delegate3.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"),
            client.data_);
}

TEST(MultipartResponseTest, MalformedBoundary) {
  // Some servers send a boundary that is prefixed by "--".  See bug 5786.

  ResourceResponse response(KURL(), "multipart/x-mixed-replace", 0, "en-US",
                            String());
  response.setHTTPHeaderField(String("Foo"), String("Bar"));
  response.setHTTPHeaderField(String("Content-type"), String("text/plain"));
  MockResourceHandleClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "--bound");

  string data(
    "--bound\n"
    "Content-type: text/plain\n\n"
    "This is a sample response\n"
    "--bound--"
    "ignore junk after end token --bound\n\nTest2\n");
  delegate.OnReceivedData(data.c_str(), static_cast<int>(data.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"), client.data_);

  delegate.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
}


// Used in for tests that break the data in various places.
struct TestChunk {
  const int start_pos;  // offset in data
  const int end_pos;    // end offset in data
  const int expected_responses;
  const int expected_received_data;
  const char* expected_data;
};

void VariousChunkSizesTest(const TestChunk chunks[], int chunks_size, int responses,
                           int received_data, const char* completed_data) {
  const string data(
    "--bound\n"                    // 0-7
    "Content-type: image/png\n\n"  // 8-32
    "datadatadatadatadata"         // 33-52
    "--bound\n"                    // 53-60
    "Content-type: image/jpg\n\n"  // 61-85
    "foofoofoofoofoo"              // 86-100
    "--bound--");                  // 101-109

  ResourceResponse response(KURL(), "multipart/x-mixed-replace", 0, "en-US",
                            String());
  MockResourceHandleClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  for (int i = 0; i < chunks_size; ++i) {
    ASSERT(chunks[i].start_pos < chunks[i].end_pos);
    string chunk = data.substr(chunks[i].start_pos,
                               chunks[i].end_pos - chunks[i].start_pos);
    delegate.OnReceivedData(chunk.c_str(), static_cast<int>(chunk.length()));
    EXPECT_EQ(chunks[i].expected_responses,
              client.received_response_);
    EXPECT_EQ(chunks[i].expected_received_data,
              client.received_data_);
    EXPECT_EQ(string(chunks[i].expected_data),
              client.data_);
  }
  // Check final state
  delegate.OnCompletedRequest();
  EXPECT_EQ(responses,
            client.received_response_);
  EXPECT_EQ(received_data,
            client.received_data_);
  EXPECT_EQ(string(completed_data),
            client.data_);
}

TEST(MultipartResponseTest, BreakInBoundary) {
  // Break in the first boundary
  const TestChunk bound1[] = {
    { 0, 4, 0, 0, ""},
    { 4, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(bound1, arraysize(bound1),
                        2, 2, "foofoofoofoofoo");

  // Break in first and second
  const TestChunk bound2[] = {
    { 0, 4, 0, 0, ""},
    { 4, 55, 1, 0, "" },
    { 55, 65, 1, 1, "datadatadatadatadata" },
    { 65, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(bound2, arraysize(bound2),
                        2, 2, "foofoofoofoofoo");

  // Break in second only
  const TestChunk bound3[] = {
    { 0, 55, 1, 0, "" },
    { 55, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(bound3, arraysize(bound3),
                        2, 2, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInHeaders) {
  // Break in first header
  const TestChunk header1[] = {
    { 0, 10, 0, 0, "" },
    { 10, 35, 1, 0, "" },
    { 35, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(header1, arraysize(header1),
                        2, 2, "foofoofoofoofoo");

  // Break in both headers
  const TestChunk header2[] = {
    { 0, 10, 0, 0, "" },
    { 10, 65, 1, 1, "datadatadatadatadata" },
    { 65, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(header2, arraysize(header2),
                        2, 2, "foofoofoofoofoo");

  // Break at end of a header
  const TestChunk header3[] = {
    { 0, 33, 1, 0, "" },
    { 33, 65, 1, 1, "datadatadatadatadata" },
    { 65, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(header3, arraysize(header3),
                        2, 2, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInData) {
  // All data as one chunk
  const TestChunk data1[] = {
    { 0, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(data1, arraysize(data1),
                        2, 2, "foofoofoofoofoo");

  // breaks in data segment
  const TestChunk data2[] = {
    { 0, 35, 1, 0, "" },
    { 35, 65, 1, 1, "datadatadatadatadata" },
    { 65, 90, 2, 1, "" },
    { 90, 110, 2, 2, "foofoofoofoofoo" },
  };
  VariousChunkSizesTest(data2, arraysize(data2),
                        2, 2, "foofoofoofoofoo");

  // Incomplete send
  const TestChunk data3[] = {
    { 0, 35, 1, 0, "" },
    { 35, 90, 2, 1, "" },
  };
  VariousChunkSizesTest(data3, arraysize(data3),
                        2, 2, "foof");
}

TEST(MultipartResponseTest, MultipleBoundaries) {
  // Test multiple boundaries back to back
  ResourceResponse response(KURL(), "multipart/x-mixed-replace", 0, "en-US",
                            String());
  MockResourceHandleClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  string data("--bound\r\n\r\n--bound\r\n\r\nfoofoo--bound--");
  delegate.OnReceivedData(data.c_str(), static_cast<int>(data.length()));
  EXPECT_EQ(2,
            client.received_response_);
  EXPECT_EQ(1,
            client.received_data_);
  EXPECT_EQ(string("foofoo"),
            client.data_);
}

TEST(MultipartResponseTest, MultipartByteRangeParsingTest) {
  // Test multipart/byteranges based boundary parsing.
  ResourceResponse response1(KURL(), "multipart/byteranges", 0, "en-US",
                            String());
  response1.setHTTPHeaderField(String("Content-Length"), String("200"));
  response1.setHTTPHeaderField(
      String("Content-type"),
      String("multipart/byteranges; boundary=--bound--"));

  std::string multipart_boundary;
  bool result = MultipartResponseDelegate::ReadMultipartBoundary(
      response1, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"),
            multipart_boundary);

  ResourceResponse response2(KURL(), "image/png", 0, "en-US",
                            String());

  response2.setHTTPHeaderField(String("Content-Length"), String("300"));
  response2.setHTTPHeaderField(
      String("Last-Modified"),
      String("Mon, 04 Apr 2005 20:36:01 GMT"));
  response2.setHTTPHeaderField(
      String("Date"),
      String("Thu, 11 Sep 2008 18:21:42 GMT"));

  multipart_boundary.clear();
  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response2, &multipart_boundary);
  EXPECT_EQ(result, false);

  ResourceResponse response3(KURL(), "multipart/byteranges", 0, "en-US",
                            String());

  response3.setHTTPHeaderField(String("Content-Length"), String("300"));
  response3.setHTTPHeaderField(
      String("Last-Modified"),
      String("Mon, 04 Apr 2005 20:36:01 GMT"));
  response3.setHTTPHeaderField(
      String("Date"),
      String("Thu, 11 Sep 2008 18:21:42 GMT"));
  response3.setHTTPHeaderField(
      String("Content-type"),
      String("multipart/byteranges"));

  multipart_boundary.clear();
  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response3, &multipart_boundary);
  EXPECT_EQ(result, false);
  EXPECT_EQ(multipart_boundary.length(), 0U);

  ResourceResponse response4(KURL(), "multipart/byteranges", 0, "en-US",
                             String());
  response4.setHTTPHeaderField(String("Content-Length"), String("200"));
  response4.setHTTPHeaderField(
      String("Content-type"),
      String("multipart/byteranges; boundary=--bound--; charSet=utf8"));

  multipart_boundary.clear();

  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response4, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"), multipart_boundary);

  ResourceResponse response5(KURL(), "multipart/byteranges", 0, "en-US",
                             String());
  response5.setHTTPHeaderField(String("Content-Length"), String("200"));
  response5.setHTTPHeaderField(
      String("Content-type"),
      String("multipart/byteranges; boundary=\"--bound--\"; charSet=utf8"));

  multipart_boundary.clear();

  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response5, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"), multipart_boundary);
}

TEST(MultipartResponseTest, MultipartContentRangesTest) {
  ResourceResponse response1(KURL(), "application/pdf", 0, "en-US",
                            String());
  response1.setHTTPHeaderField(String("Content-Length"), String("200"));
  response1.setHTTPHeaderField(
      String("Content-Range"),
      String("bytes 1000-1050/5000"));

  int content_range_lower_bound = 0;
  int content_range_upper_bound = 0;

  bool result = MultipartResponseDelegate::ReadContentRanges(
      response1, &content_range_lower_bound,
      &content_range_upper_bound);

  EXPECT_EQ(result, true);
  EXPECT_EQ(content_range_lower_bound, 1000);
  EXPECT_EQ(content_range_upper_bound, 1050);

  ResourceResponse response2(KURL(), "application/pdf", 0, "en-US",
                            String());
  response2.setHTTPHeaderField(String("Content-Length"), String("200"));
  response2.setHTTPHeaderField(
      String("Content-Range"),
      String("bytes 1000/1050"));

  content_range_lower_bound = 0;
  content_range_upper_bound = 0;

  result = MultipartResponseDelegate::ReadContentRanges(
      response2, &content_range_lower_bound,
      &content_range_upper_bound);

  EXPECT_EQ(result, false);
}

}  // namespace
