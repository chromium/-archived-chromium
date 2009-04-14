// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the bindings for opening a channel to an extension and sending
// and receiving messages through that channel all works.
TEST_F(RenderViewTest, ExtensionMessagesOpenChannel) {
  render_thread_.sink().ClearMessages();
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "var e = new chromium.Extension('foobar');"
    "var port = e.connect();"
    "port.onmessage.addListener(doOnMessage);"
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
  RendererExtensionBindings::HandleMessage("{\"val\": 42}", kPortId);

  // Verify that we got it.
  const IPC::Message* alert_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ViewHostMsg_RunJavaScriptMessage::SendParam alert_param;
  IPC::ReadParam(alert_msg, &iter, &alert_param);
  EXPECT_EQ(L"content got: 42", alert_param.a);
}

// Tests that the bindings for handling a new channel connection and sending
// and receiving messages through that channel all works.
TEST_F(RenderViewTest, ExtensionMessagesOnConnect) {
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "chromium.onconnect.addListener(function (port) {"
    "  port.onmessage.addListener(doOnMessage);"
    "  port.postMessage({message: 'onconnect'});"
    "});"
    "function doOnMessage(msg, port) {"
    "  alert('got: ' + msg.val);"
    "}");

  render_thread_.sink().ClearMessages();

  // Simulate a new connection being opened.
  const int kPortId = 0;
  RendererExtensionBindings::HandleConnect(kPortId);

  // Verify that we handled the new connection by posting a message.
  const IPC::Message* post_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionPostMessage::ID);
  ASSERT_TRUE(post_msg);
  ViewHostMsg_ExtensionPostMessage::Param post_params;
  ViewHostMsg_ExtensionPostMessage::Read(post_msg, &post_params);
  EXPECT_EQ("{\"message\":\"onconnect\"}", post_params.b);

  // Now simulate getting a message back from the channel opener.
  render_thread_.sink().ClearMessages();
  RendererExtensionBindings::HandleMessage("{\"val\": 42}", kPortId);

  // Verify that we got it.
  const IPC::Message* alert_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ViewHostMsg_RunJavaScriptMessage::SendParam alert_param;
  IPC::ReadParam(alert_msg, &iter, &alert_param);
  EXPECT_EQ(L"got: 42", alert_param.a);
}
