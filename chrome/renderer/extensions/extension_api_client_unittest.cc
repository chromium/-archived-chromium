// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionAPIClientTest : public RenderViewTest {
 protected:
  virtual void SetUp() {
    RenderViewTest::SetUp();

    render_thread_.sink().ClearMessages();
    LoadHTML("<body></body>");
  }

  std::string GetConsoleMessage() {
    const IPC::Message* message =
        render_thread_.sink().GetUniqueMessageMatching(
            ViewHostMsg_AddMessageToConsole::ID);
    ViewHostMsg_AddMessageToConsole::Param params;
    if (message) {
      ViewHostMsg_AddMessageToConsole::Read(message, &params);
      render_thread_.sink().ClearMessages();
      return WideToASCII(params.a);
    } else {
      return "";
    }
  }

  void ExpectJsFail(const std::string& js, const std::string& message) {
    ExecuteJavaScript(js.c_str());
    EXPECT_EQ(message, GetConsoleMessage());
  }
};

// Tests that callback dispatching works correctly and that JSON is properly
// deserialized before handing off to the extension code. We use the createTab
// API here, but we could use any of them since they all dispatch callbacks the
// same way.
TEST_F(ExtensionAPIClientTest, CallbackDispatching) {
  ExecuteJavaScript(
    "function assert(truth, message) {"
    "  if (!truth) {"
    "    throw new Error(message);"
    "  }"
    "}"
    "function callback(result) {"
    "  assert(typeof result == 'object', 'result not object');"
    "  assert(goog.json.serialize(result) == '{\"foo\":\"bar\"}', "
    "         'incorrect result');"
    "  console.log('pass')"
    "}"
    "chromium.tabs.createTab({}, callback);"
  );

  // Ok, we should have gotten a message to create a tab, grab the callback ID.
  const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  int callback_id = params.c;
  ASSERT_TRUE(callback_id >= 0);

  // Now send the callback a response
  ExtensionProcessBindings::ExecuteCallbackInFrame(
    GetMainFrame(), callback_id, "{\"foo\":\"bar\"}");

  // And verify that it worked
  ASSERT_EQ("pass", GetConsoleMessage());
}

// The remainder of these tests exercise the client side of the various
// extension functions. We test both error and success conditions, but do not
// test errors exhaustively as json schema code is well tested by itself.

TEST_F(ExtensionAPIClientTest, GetTabsForWindow) {
  ExpectJsFail("chromium.tabs.getTabsForWindow(42, function(){});",
               "Uncaught Error: Too many arguments.");

  ExecuteJavaScript("chromium.tabs.getTabsForWindow(function(){})");
  const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  ASSERT_EQ("GetTabsForWindow", params.a);
  ASSERT_EQ("null", params.b);
}

TEST_F(ExtensionAPIClientTest, GetTab) {
  ExpectJsFail("chromium.tabs.getTab(null, function(){});",
               "Uncaught Error: Argument 0 is required.");

  ExecuteJavaScript("chromium.tabs.getTab(42)");
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  ASSERT_EQ("GetTab", params.a);
  ASSERT_EQ("42", params.b);
}

TEST_F(ExtensionAPIClientTest, CreateTab) {
  ExpectJsFail("chromium.tabs.createTab({windowId: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'windowId': Expected 'integer' but got 'string'.");
  ExpectJsFail("chromium.tabs.createTab({url: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");
  ExpectJsFail("chromium.tabs.createTab({selected: null}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'selected': Expected 'boolean' but got 'null'.");

  ExecuteJavaScript("chromium.tabs.createTab({"
                    "  url:'http://www.google.com/',"
                    "  selected:true,"
                    "  windowId:4"
                    "})");
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  ASSERT_EQ("CreateTab", params.a);
  ASSERT_EQ("{\"url\":\"http://www.google.com/\","
             "\"selected\":true,"
             "\"windowId\":4}", params.b);
}

TEST_F(ExtensionAPIClientTest, UpdateTab) {
  ExpectJsFail("chromium.tabs.updateTab({id: null});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'id': Expected 'integer' but got 'null'.");
  ExpectJsFail("chromium.tabs.updateTab({id: 42, windowId: 'foo'});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'windowId': Expected 'integer' but got 'string'.");
  ExpectJsFail("chromium.tabs.updateTab({id: 42, url: 42});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");
  ExpectJsFail("chromium.tabs.updateTab({id: 42, selected: null});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'selected': Expected 'boolean' but got 'null'.");

  ExecuteJavaScript("chromium.tabs.updateTab({"
                    "  id:42,"
                    "  url:'http://www.google.com/',"
                    "  selected:true,"
                    "  windowId:4"
                    "})");
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  ASSERT_EQ("UpdateTab", params.a);
  ASSERT_EQ("{\"id\":42,"
             "\"url\":\"http://www.google.com/\","
             "\"selected\":true,"
             "\"windowId\":4}", params.b);
}

TEST_F(ExtensionAPIClientTest, RemoveTab) {
  ExpectJsFail("chromium.tabs.removeTab('foobar', function(){});",
               "Uncaught Error: Too many arguments.");

  ExecuteJavaScript("chromium.tabs.removeTab(21)");
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  ASSERT_EQ("RemoveTab", params.a);
  ASSERT_EQ("21", params.b);
}
