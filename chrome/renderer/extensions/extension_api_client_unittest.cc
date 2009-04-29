// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
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
    render_thread_.sink().ClearMessages();
  }

  void ExpectJsPass(const std::string& js,
                    const std::string& function,
                    const std::string& arg1) {
    ExecuteJavaScript(js.c_str());
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
    ASSERT_EQ("", GetConsoleMessage()) << js;
    ASSERT_TRUE(request_msg) << js;
    ViewHostMsg_ExtensionRequest::Param params;
    ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
    ASSERT_EQ(function.c_str(), params.a) << js;
    ASSERT_EQ(arg1.c_str(), params.b) << js;
    render_thread_.sink().ClearMessages();
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

  EXPECT_EQ("", GetConsoleMessage());

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

  ExpectJsPass("chromium.tabs.getTabsForWindow(function(){})",
               "GetTabsForWindow", "null");
}

TEST_F(ExtensionAPIClientTest, GetTab) {
  ExpectJsFail("chromium.tabs.getTab(null, function(){});",
               "Uncaught Error: Parameter 0 is required.");

  ExpectJsPass("chromium.tabs.getTab(42)", "GetTab", "42");
}

TEST_F(ExtensionAPIClientTest, CreateTab) {
  ExpectJsFail("chromium.tabs.createTab({windowId: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'windowId': Expected 'integer' but got 'string'.");
  ExpectJsFail("chromium.tabs.createTab({url: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");
  ExpectJsFail("chromium.tabs.createTab({foo: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'foo': Unexpected property.");

  ExpectJsPass("chromium.tabs.createTab({"
               "  url:'http://www.google.com/',"
               "  selected:true,"
               "  windowId:4"
               "})",
               "CreateTab",
               "{\"url\":\"http://www.google.com/\","
               "\"selected\":true,"
               "\"windowId\":4}");
}

TEST_F(ExtensionAPIClientTest, UpdateTab) {
  ExpectJsFail("chromium.tabs.updateTab({id: null});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'id': Property is required.");
  ExpectJsFail("chromium.tabs.updateTab({id: 42, windowId: 'foo'});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'windowId': Expected 'integer' but got 'string'.");
  ExpectJsFail("chromium.tabs.updateTab({id: 42, url: 42});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");

  ExpectJsPass("chromium.tabs.updateTab({"
               "  id:42,"
               "  url:'http://www.google.com/',"
               "  selected:true,"
               "  windowId:4"
               "})",
               "UpdateTab",
               "{\"id\":42,"
               "\"url\":\"http://www.google.com/\","
               "\"selected\":true,"
               "\"windowId\":4}");
}

TEST_F(ExtensionAPIClientTest, RemoveTab) {
  ExpectJsFail("chromium.tabs.removeTab('foobar', function(){});",
               "Uncaught Error: Too many arguments.");

  ExpectJsPass("chromium.tabs.removeTab(21)", "RemoveTab", "21");
}

// Bookmark API tests
// TODO(erikkay) add more variations here

TEST_F(ExtensionAPIClientTest, CreateBookmark) {
  ExpectJsFail(
      "chromium.bookmarks.create({parentId:'x', title:0}, function(){})",
      "Uncaught Error: Invalid value for argument 0. "
      "Property 'parentId': Expected 'integer' but got 'string', "
      "Property 'title': Expected 'string' but got 'integer'.");

  ExpectJsPass(
      "chromium.bookmarks.create({parentId:0, title:'x'}, function(){})",
      "CreateBookmark",
      "{\"parentId\":0,\"title\":\"x\"}");
}

TEST_F(ExtensionAPIClientTest, GetBookmarks) {
  ExpectJsPass("chromium.bookmarks.get([], function(){});",
               "GetBookmarks",
               "[]");
  ExpectJsPass("chromium.bookmarks.get([0,1,2,3], function(){});",
               "GetBookmarks",
               "[0,1,2,3]");
  ExpectJsPass("chromium.bookmarks.get(null, function(){});",
               "GetBookmarks",
               "null");
  ExpectJsFail("chromium.bookmarks.get({}, function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'array' but got 'object'.");
}

TEST_F(ExtensionAPIClientTest, GetBookmarkChildren) {
  ExpectJsPass("chromium.bookmarks.getChildren(42, function(){});",
               "GetBookmarkChildren",
               "42");
}

TEST_F(ExtensionAPIClientTest, GetBookmarkTree) {
  ExpectJsPass("chromium.bookmarks.getTree(function(){});",
               "GetBookmarkTree",
               "null");
}

TEST_F(ExtensionAPIClientTest, SearchBookmarks) {
  ExpectJsPass("chromium.bookmarks.search('hello',function(){});",
               "SearchBookmarks",
               "\"hello\"");
}

TEST_F(ExtensionAPIClientTest, RemoveBookmark) {
  ExpectJsPass("chromium.bookmarks.remove({id:42});",
               "RemoveBookmark",
               "{\"id\":42}");
}

TEST_F(ExtensionAPIClientTest, MoveBookmark) {
  ExpectJsPass("chromium.bookmarks.move({id:42,parentId:1,index:0});",
               "MoveBookmark",
               "{\"id\":42,\"parentId\":1,\"index\":0}");
}

TEST_F(ExtensionAPIClientTest, SetBookmarkTitle) {
  ExpectJsPass("chromium.bookmarks.setTitle({id:42,title:'x'});",
               "SetBookmarkTitle",
               "{\"id\":42,\"title\":\"x\"}");
}

