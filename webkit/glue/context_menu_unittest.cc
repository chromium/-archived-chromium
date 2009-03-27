// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for displaying context menus in corner cases (and swallowing context
// menu events when appropriate)

#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

// Right clicking inside on an iframe should produce a context menu
class ContextMenuCapturing : public TestShellTest {
 protected:
  void SetUp() {
    TestShellTest::SetUp();

    iframes_data_dir_ = data_dir_;
    file_util::AppendToPath(&iframes_data_dir_, L"test_shell");
    file_util::AppendToPath(&iframes_data_dir_, L"iframes");
    ASSERT_TRUE(file_util::PathExists(iframes_data_dir_));
  }

  std::wstring iframes_data_dir_;
};


TEST_F(ContextMenuCapturing, ContextMenuCapturing) {
  // Make sure we have no stored mouse event state
  WebViewDelegate* raw_delegate = test_shell_->webView()->GetDelegate();
  TestWebViewDelegate* test_delegate = static_cast<TestWebViewDelegate*>(raw_delegate);
  test_delegate->clear_captured_context_menu_events();
  EXPECT_EQ(0U, test_delegate->captured_context_menu_events().size());

  std::wstring test_url = GetTestURL(iframes_data_dir_, L"testiframe.html");
  test_shell_->LoadURL(test_url.c_str());
  test_shell_->WaitTestFinished();

  // Create a right click in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other bug.)
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MouseDown;
  mouse_event.button = WebMouseEvent::ButtonRight;
  mouse_event.x = 250;
  mouse_event.y = 250;
  mouse_event.globalX = 250;
  mouse_event.globalY = 250;

  WebView* webview = test_shell_->webView();
  webview->HandleInputEvent(&mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.type = WebInputEvent::MouseUp;
  webview->HandleInputEvent(&mouse_event);

  EXPECT_EQ(1U, test_delegate->captured_context_menu_events().size());
}
