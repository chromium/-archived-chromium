// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceRequest.h"
#include "CString.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/webplugin_impl.h"

namespace {

class WebPluginImplTest : public testing::Test {
};

}

// These exist only to support the gTest assertion macros, and
// shouldn't be used in normal program code.
std::ostream& operator<<(std::ostream& out, const WebCore::String& str)
{
  return out << str.latin1().data();
}

// The Host functions for NPN_PostURL and NPN_PostURLNotify
// need to parse out some HTTP headers.  Make sure it works
// with the following tests

TEST(WebPluginImplTest, PostParserSimple) {
  // Test a simple case with headers & data
  const char *ex1 = "foo: bar\nContent-length: 10\n\nabcdefghij";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ(0U, request.httpHeaderField("Content-length").length());
  EXPECT_EQ("abcdefghij", request.httpBody()->flattenToString());
}

TEST(WebPluginImplTest, PostParserLongHeader) {
  // Test a simple case with long headers
  const char *ex1 = "foo: 0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n\nabcdefghij";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(100U, request.httpHeaderField("foo").stripWhiteSpace().length());
}

TEST(WebPluginImplTest, PostParserManyHeaders) {
  // Test a simple case with long headers
  const char *ex1 = "h1:h1\nh2:h2\nh3:h3\nh4:h4\nh5:h5\nh6:h6\nh7:h7\nh8:h8\nh9:h9\nh10:h10\n\nbody";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("h1", request.httpHeaderField("h1").stripWhiteSpace());
  EXPECT_EQ("h2", request.httpHeaderField("h2").stripWhiteSpace());
  EXPECT_EQ("h3", request.httpHeaderField("h3").stripWhiteSpace());
  EXPECT_EQ("h4", request.httpHeaderField("h4").stripWhiteSpace());
  EXPECT_EQ("h5", request.httpHeaderField("h5").stripWhiteSpace());
  EXPECT_EQ("h6", request.httpHeaderField("h6").stripWhiteSpace());
  EXPECT_EQ("h7", request.httpHeaderField("h7").stripWhiteSpace());
  EXPECT_EQ("h8", request.httpHeaderField("h8").stripWhiteSpace());
  EXPECT_EQ("h9", request.httpHeaderField("h9").stripWhiteSpace());
  EXPECT_EQ("h10", request.httpHeaderField("h10").stripWhiteSpace());
  WebCore::FormData *form_data = request.httpBody();
  WebCore::String string_data = form_data->flattenToString();
  EXPECT_EQ(string_data, "body");
}

TEST(WebPluginImplTest, PostParserDuplicateHeaders) {
  // Test a simple case with long headers
  // What value gets returned doesn't really matter.  It shouldn't error
  // out.
  const char *ex1 = "h1:h1\nh1:h2\n\nbody";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserNoHeaders) {
  // Test a simple case with no headers but with data
  const char *ex1 = "\nabcdefghij";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(0U, request.httpHeaderField("foo").length());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ(0U, request.httpHeaderField("Content-length").length());
  EXPECT_EQ("abcdefghij", request.httpBody()->flattenToString());
}

TEST(WebPluginImplTest, PostParserNoBody) {
  // Test a simple case with headers and no body
  const char *ex1 = "Foo:bar\n\n";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ(0U, request.httpHeaderField("Content-length").length());
  EXPECT_EQ(0U, request.httpBody()->flattenToString().length());
}

TEST(WebPluginImplTest, PostParserBodyWithNewLines) {
  // Test a simple case with headers and no body
  const char *ex1 = "Foo:bar\n\n\n\nabcdefg\n\nabcdefg";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(request.httpBody()->flattenToString(), "\n\nabcdefg\n\nabcdefg");
}

TEST(WebPluginImplTest, PostParserErrorNoBody) {
  // Test with headers and no body
  const char *ex1 = "Foo:bar\n";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserErrorEmpty) {
  // Test with an empty string
  const char *ex1 = "";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserEmptyName) {
  // Test an error case with an empty header name field
  const char *ex1 = "foo:bar\n:blat\n\nbody";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ("body", request.httpBody()->flattenToString());
}

TEST(WebPluginImplTest, PostParserEmptyValue) {
  // Test an error case with an empty value field
  const char *ex1 = "foo:bar\nbar:\n\nbody";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ("body", request.httpBody()->flattenToString());
}

TEST(WebPluginImplTest, PostParserCRLF) {
  // Test an error case with an empty value field
  const char *ex1 = "foo: bar\r\nbar:\r\n\r\nbody\r\n\r\nbody2";
  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ("body\r\n\r\nbody2", request.httpBody()->flattenToString());
}

TEST(WebPluginImplTest, PostParserBodyWithBinaryData) {
  // Test a simple case with headers and binary data.
  char ex1[33] = "foo: bar\nContent-length: 10\n\n";
  unsigned int binary_data = 0xFFFFFFF0;
  memcpy(ex1 + strlen("foo: bar\nContent-length: 10\n\n"), &binary_data,
        sizeof(binary_data));

  WebCore::ResourceRequest request;
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                      sizeof(ex1)/sizeof(ex1[0]));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", request.httpHeaderField("foo").stripWhiteSpace());
  EXPECT_EQ(0U, request.httpHeaderField("bar").length());
  EXPECT_EQ(0U, request.httpHeaderField("Content-length").length());

  Vector<char> expected_data;
  request.httpBody()->flatten(expected_data);

  EXPECT_EQ(0xF0, (unsigned char)expected_data[0]);
  EXPECT_EQ(0xFF, (unsigned char)expected_data[1]);
  EXPECT_EQ(0xFF, (unsigned char)expected_data[2]);
  EXPECT_EQ(0xFF, (unsigned char)expected_data[3]);
}
