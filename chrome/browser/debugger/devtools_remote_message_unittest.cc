// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "chrome/browser/debugger/devtools_remote.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsRemoteMessageTest : public testing::Test {
 public:
  DevToolsRemoteMessageTest() : testing::Test() {
  }

 protected:
  virtual void SetUp() {
    testing::Test::SetUp();
  }
};

TEST_F(DevToolsRemoteMessageTest, ConstructInstanceManually) {
  DevToolsRemoteMessage::HeaderMap headers;
  std::string content = "{\"command\":\"ping\"}";
  headers[DevToolsRemoteMessageHeaders::kTool] = "DevToolsService";
  headers[DevToolsRemoteMessageHeaders::kContentLength] =
      IntToString(content.size());

  DevToolsRemoteMessage message(headers, content);
  ASSERT_STREQ("DevToolsService",
               message.GetHeaderWithEmptyDefault(
                   DevToolsRemoteMessageHeaders::kTool).c_str());
  ASSERT_STREQ("DevToolsService", message.tool().c_str());
  ASSERT_STREQ(content.c_str(), message.content().c_str());
  ASSERT_EQ(content.size(),
            static_cast<std::string::size_type>(message.content_length()));
  ASSERT_EQ(static_cast<DevToolsRemoteMessage::HeaderMap::size_type>(2),
            message.headers().size());
}

TEST_F(DevToolsRemoteMessageTest, ConstructWithBuilder) {
  std::string content = "Responsecontent";
  testing::internal::scoped_ptr<DevToolsRemoteMessage> message(
      DevToolsRemoteMessageBuilder::instance().Create(
          "V8Debugger",  // tool
          "2",  // destination
          content));  // content

  ASSERT_EQ(static_cast<DevToolsRemoteMessage::HeaderMap::size_type>(3),
            message->headers().size());
  ASSERT_STREQ(
      "V8Debugger",
      message->GetHeaderWithEmptyDefault(
          DevToolsRemoteMessageHeaders::kTool).c_str());
  ASSERT_STREQ(
      "V8Debugger",
      message->tool().c_str());
  ASSERT_STREQ(
      "2",
      message->GetHeaderWithEmptyDefault(
          DevToolsRemoteMessageHeaders::kDestination).c_str());
  ASSERT_STREQ(
      "2",
      message->destination().c_str());
  ASSERT_EQ(content.size(),
      static_cast<DevToolsRemoteMessage::HeaderMap::size_type>(
          message->content_length()));
  ASSERT_STREQ(content.c_str(), message->content().c_str());
}
