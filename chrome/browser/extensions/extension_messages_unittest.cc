// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

static void DispatchOnConnect(int source_port_id, const std::string& tab_json) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  args.Set(1, Value::CreateStringValue(tab_json));
  args.Set(2, Value::CreateStringValue(""));  // extension ID is empty for tests
  RendererExtensionBindings::Invoke(
      ExtensionMessageService::kDispatchOnConnect, args);
}

static void DispatchOnDisconnect(int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  RendererExtensionBindings::Invoke(
      ExtensionMessageService::kDispatchOnDisconnect, args);
}

static void DispatchOnMessage(const std::string& message, int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(message));
  args.Set(1, Value::CreateIntegerValue(source_port_id));
  RendererExtensionBindings::Invoke(
      ExtensionMessageService::kDispatchOnMessage, args);
}

// Tests that the bindings for opening a channel to an extension and sending
// and receiving messages through that channel all works.
TEST_F(RenderViewTest, ExtensionMessagesOpenChannel) {
  render_thread_.sink().ClearMessages();
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "var e = new chrome.Extension('foobar');"
    "var port = e.connect();"
    "port.onMessage.addListener(doOnMessage);"
    "port.postMessage({message: 'content ready'});"
    "function doOnMessage(msg, port) {"
    "  alert('content got: ' + msg.val);"
    "}");

  // Verify that we opened a channel and sent a message through it.
  const IPC::Message* open_channel_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_OpenChannelToExtension::ID);
  EXPECT_TRUE(open_channel_msg);

  const IPC::Message* post_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionPostMessage::ID);
  ASSERT_TRUE(post_msg);
  ViewHostMsg_ExtensionPostMessage::Param post_params;
  ViewHostMsg_ExtensionPostMessage::Read(post_msg, &post_params);
  EXPECT_EQ("{\"message\":\"content ready\"}", post_params.b);

  // Now simulate getting a message back from the other side.
  render_thread_.sink().ClearMessages();
  const int kPortId = 0;
  DispatchOnMessage("{\"val\": 42}", kPortId);

  // Verify that we got it.
  const IPC::Message* alert_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ViewHostMsg_RunJavaScriptMessage::SendParam alert_param;
  ASSERT_TRUE(IPC::ReadParam(alert_msg, &iter, &alert_param));
  EXPECT_EQ(L"content got: 42", alert_param.a);
}

// Tests that the bindings for handling a new channel connection and channel
// closing all works.
TEST_F(RenderViewTest, ExtensionMessagesOnConnect) {
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "chrome.self.onConnect.addListener(function (port) {"
    "  port.test = 24;"
    "  port.onMessage.addListener(doOnMessage);"
    "  port.onDisconnect.addListener(doOnDisconnect);"
    "  port.postMessage({message: 'onconnect from ' + port.tab.url});"
    "});"
    "function doOnMessage(msg, port) {"
    "  alert('got: ' + msg.val);"
    "}"
    "function doOnDisconnect(port) {"
    "  alert('disconnected: ' + port.test);"
    "}");

  render_thread_.sink().ClearMessages();

  // Simulate a new connection being opened.
  const int kPortId = 0;
  DispatchOnConnect(kPortId, "{\"url\":\"foo://bar\"}");

  // Verify that we handled the new connection by posting a message.
  const IPC::Message* post_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionPostMessage::ID);
  ASSERT_TRUE(post_msg);
  ViewHostMsg_ExtensionPostMessage::Param post_params;
  ViewHostMsg_ExtensionPostMessage::Read(post_msg, &post_params);
  EXPECT_EQ("{\"message\":\"onconnect from foo://bar\"}", post_params.b);

  // Now simulate getting a message back from the channel opener.
  render_thread_.sink().ClearMessages();
  DispatchOnMessage("{\"val\": 42}", kPortId);

  // Verify that we got it.
  const IPC::Message* alert_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ViewHostMsg_RunJavaScriptMessage::SendParam alert_param;
  ASSERT_TRUE(IPC::ReadParam(alert_msg, &iter, &alert_param));
  EXPECT_EQ(L"got: 42", alert_param.a);

  // Now simulate the channel closing.
  render_thread_.sink().ClearMessages();
  DispatchOnDisconnect(kPortId);

  // Verify that we got it.
  alert_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ASSERT_TRUE(IPC::ReadParam(alert_msg, &iter, &alert_param));
  EXPECT_EQ(L"disconnected: 24", alert_param.a);
}
