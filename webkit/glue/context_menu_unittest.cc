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

// Tests for displaying context menus in corner cases (and swallowing context 
// menu events when appropriate)

#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"


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
  EXPECT_EQ(0, test_delegate->captured_context_menu_events().size());

  std::wstring test_url = GetTestURL(iframes_data_dir_, L"testiframe.html");
  test_shell_->LoadURL(test_url.c_str());
  test_shell_->WaitTestFinished();
  
  // Create a right click in the center of the iframe. (I'm hoping this will 
  // make this a bit more robust in case of some other formatting or other bug.)
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MOUSE_DOWN;
  mouse_event.modifiers = 0;
  mouse_event.button = WebMouseEvent::BUTTON_RIGHT;
  mouse_event.x = 250;
  mouse_event.y = 250;
  mouse_event.global_x = 250;
  mouse_event.global_y = 250;
  mouse_event.timestamp_sec = 0;
  mouse_event.layout_test_click_count = 0;

  WebView* webview = test_shell_->webView();
  webview->HandleInputEvent(&mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.type = WebInputEvent::MOUSE_UP;  
  webview->HandleInputEvent(&mouse_event);

  EXPECT_EQ(1, test_delegate->captured_context_menu_events().size());
}
