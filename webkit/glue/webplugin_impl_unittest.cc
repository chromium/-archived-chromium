// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

// Avoid collisions with the LOG macro
#include <wtf/Assertions.h>
#undef LOG

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin_impl.h"

using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebURLRequest;

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

static std::string GetHeader(const WebURLRequest& request, const char* name) {
  std::string result;
  TrimWhitespace(
      webkit_glue::WebStringToStdString(
          request.httpHeaderField(WebString::fromUTF8(name))),
      TRIM_ALL,
      &result);
  return result;
}

static std::string GetBodyText(const WebURLRequest& request) {
  const WebHTTPBody& body = request.httpBody();
  if (body.isNull())
    return std::string();

  std::string result;
  size_t i = 0;
  WebHTTPBody::Element element;
  while (body.elementAt(i++, element)) {
    if (element.type == WebHTTPBody::Element::TypeData) {
      result.append(element.data.data(), element.data.size());
    } else {
      NOTREACHED() << "unexpected element type encountered!";
    }
  }
  return result;
}

// The Host functions for NPN_PostURL and NPN_PostURLNotify
// need to parse out some HTTP headers.  Make sure it works
// with the following tests

TEST(WebPluginImplTest, PostParserSimple) {
  // Test a simple case with headers & data
  const char *ex1 = "foo: bar\nContent-length: 10\n\nabcdefghij";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ(0U, GetHeader(request, "Content-length").length());
  EXPECT_EQ("abcdefghij", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserLongHeader) {
  // Test a simple case with long headers
  const char *ex1 = "foo: 0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n\nabcdefghij";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(100U, GetHeader(request, "foo").length());
}

TEST(WebPluginImplTest, PostParserManyHeaders) {
  // Test a simple case with long headers
  const char *ex1 = "h1:h1\nh2:h2\nh3:h3\nh4:h4\nh5:h5\nh6:h6\nh7:h7\nh8:h8\nh9:h9\nh10:h10\n\nbody";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("h1", GetHeader(request, "h1"));
  EXPECT_EQ("h2", GetHeader(request, "h2"));
  EXPECT_EQ("h3", GetHeader(request, "h3"));
  EXPECT_EQ("h4", GetHeader(request, "h4"));
  EXPECT_EQ("h5", GetHeader(request, "h5"));
  EXPECT_EQ("h6", GetHeader(request, "h6"));
  EXPECT_EQ("h7", GetHeader(request, "h7"));
  EXPECT_EQ("h8", GetHeader(request, "h8"));
  EXPECT_EQ("h9", GetHeader(request, "h9"));
  EXPECT_EQ("h10", GetHeader(request, "h10"));
  EXPECT_EQ("body", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserDuplicateHeaders) {
  // Test a simple case with long headers
  // What value gets returned doesn't really matter.  It shouldn't error
  // out.
  const char *ex1 = "h1:h1\nh1:h2\n\nbody";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserNoHeaders) {
  // Test a simple case with no headers but with data
  const char *ex1 = "\nabcdefghij";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(0U, GetHeader(request, "foo").length());
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ(0U, GetHeader(request, "Content-length").length());
  EXPECT_EQ("abcdefghij", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserNoBody) {
  // Test a simple case with headers and no body
  const char *ex1 = "Foo:bar\n\n";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ(0U, GetHeader(request, "Content-length").length());
  EXPECT_EQ(0U, GetBodyText(request).length());
}

TEST(WebPluginImplTest, PostParserBodyWithNewLines) {
  // Test a simple case with headers and no body
  const char *ex1 = "Foo:bar\n\n\n\nabcdefg\n\nabcdefg";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ(GetBodyText(request), "\n\nabcdefg\n\nabcdefg");
}

TEST(WebPluginImplTest, PostParserErrorNoBody) {
  // Test with headers and no body
  const char *ex1 = "Foo:bar\n";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserErrorEmpty) {
  // Test with an empty string
  const char *ex1 = "";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
}

TEST(WebPluginImplTest, PostParserEmptyName) {
  // Test an error case with an empty header name field
  const char *ex1 = "foo:bar\n:blat\n\nbody";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ("body", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserEmptyValue) {
  // Test an error case with an empty value field
  const char *ex1 = "foo:bar\nbar:\n\nbody";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ("body", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserCRLF) {
  // Test an error case with an empty value field
  const char *ex1 = "foo: bar\r\nbar:\r\n\r\nbody\r\n\r\nbody2";
  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                       static_cast<uint32>(strlen(ex1)));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ("body\r\n\r\nbody2", GetBodyText(request));
}

TEST(WebPluginImplTest, PostParserBodyWithBinaryData) {
  // Test a simple case with headers and binary data.
  char ex1[33] = "foo: bar\nContent-length: 10\n\n";
  unsigned int binary_data = 0xFFFFFFF0;
  memcpy(ex1 + strlen("foo: bar\nContent-length: 10\n\n"), &binary_data,
        sizeof(binary_data));

  WebURLRequest request;
  request.initialize();
  bool rv = WebPluginImpl::SetPostData(&request, ex1,
                                      sizeof(ex1)/sizeof(ex1[0]));
  EXPECT_EQ(true, rv);
  EXPECT_EQ("bar", GetHeader(request, "foo"));
  EXPECT_EQ(0U, GetHeader(request, "bar").length());
  EXPECT_EQ(0U, GetHeader(request, "Content-length").length());

  std::string body = GetBodyText(request);

  EXPECT_EQ(0xF0, (unsigned char)body[0]);
  EXPECT_EQ(0xFF, (unsigned char)body[1]);
  EXPECT_EQ(0xFF, (unsigned char)body[2]);
  EXPECT_EQ(0xFF, (unsigned char)body[3]);
}
