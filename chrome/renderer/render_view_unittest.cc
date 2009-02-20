// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/mock_render_thread.h"
#include "chrome/renderer/render_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"

namespace {

const int32 kRouteId = 5;
const int32 kOpenerId = 7;

};

class RenderViewTest : public testing::Test {
 public:
  RenderViewTest() {}
  ~RenderViewTest() {}

 protected:
  // Spins the message loop to process all messages that are currently pending.
  void ProcessPendingMessages() {
    msg_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
    msg_loop_.Run();
  }

  // Returns a pointer to the main frame.
  WebFrame* GetMainFrame() {
    return view_->webview()->GetMainFrame();
  }

  // Executes the given JavaScript in the context of the main frame. The input
  // is a NULL-terminated UTF-8 string.
  void ExecuteJavaScript(const char* js) {
    GetMainFrame()->ExecuteJavaScript(js,
                                      GURL(), // script url
                                      1); // base line number
  }

  // Loads the given HTML into the main frame as a data: URL.
  void LoadHTML(const char* html) {
    std::string url_str = "data:text/html;charset=utf-8,";
    url_str.append(html);
    GURL url(url_str);

    scoped_ptr<WebRequest> request(WebRequest::Create(url));
    GetMainFrame()->LoadRequest(request.get());

    // The load actually happens asynchronously, so we pump messages to process
    // the pending continuation.
    ProcessPendingMessages();
  }

  // testing::Test
  virtual void SetUp() {
    mock_process_.reset(new MockProcess());

    render_thread_.set_routing_id(kRouteId);

    // This needs to pass the mock render thread to the view.
    view_ = RenderView::Create(&render_thread_, NULL, NULL, kOpenerId,
                               WebPreferences(),
                               new SharedRenderViewCounter(0), kRouteId);
  }
  virtual void TearDown() {
    render_thread_.SendCloseMessage();

    // Run the loop so the release task from the renderwidget executes.
    ProcessPendingMessages();

    view_ = NULL;

    mock_process_.reset();
  }

  MessageLoop msg_loop_;
  MockRenderThread render_thread_;
  scoped_ptr<MockProcess> mock_process_;
  scoped_refptr<RenderView> view_;
};


TEST_F(RenderViewTest, OnLoadAlternateHTMLText) {
  // Test a new navigation.
  GURL test_url("http://www.google.com/some_test_url");
  view_->OnLoadAlternateHTMLText("<html></html>", true, test_url,
                                 std::string());

  // We should have gotten two different types of start messages in the
  // following order.
  ASSERT_EQ((size_t)2, render_thread_.sink().message_count());
  const IPC::Message* msg = render_thread_.sink().GetMessageAt(0);
  EXPECT_EQ(ViewHostMsg_DidStartLoading::ID, msg->type());

  msg = render_thread_.sink().GetMessageAt(1);
  EXPECT_EQ(ViewHostMsg_DidStartProvisionalLoadForFrame::ID, msg->type());
  ViewHostMsg_DidStartProvisionalLoadForFrame::Param start_params;
  ViewHostMsg_DidStartProvisionalLoadForFrame::Read(msg, &start_params);
  EXPECT_EQ(GURL("chrome-ui://chromewebdata/"), start_params.b);
}

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewTest, OnNavStateChanged) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view_->set_delay_seconds_for_form_state_sync(0);

  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateState::ID));
  render_thread_.sink().ClearMessages();

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScript("document.getElementById('elt_text').value = 'foo';");
  ProcessPendingMessages();
  EXPECT_TRUE(render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID));
}

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewTest, OnImeStateChanged) {
  // Enable our IME backend code.
  view_->OnImeSetInputMode(true);

  // Load an HTML page consisting of two input fields.
  view_->set_delay_seconds_for_form_state_sync(0);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" type=\"text\"></input>"
           "<input id=\"test2\" type=\"password\"></input>"
           "</body>"
           "</html>");
  render_thread_.sink().ClearMessages();

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScript("document.getElementById('test1').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    view_->UpdateIME();
    const IPC::Message* msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateStatus::ID, msg->type());
    ViewHostMsg_ImeUpdateStatus::Param params;
    ViewHostMsg_ImeUpdateStatus::Read(msg, &params);
    EXPECT_EQ(params.a, IME_COMPLETE_COMPOSITION);
    EXPECT_TRUE(params.b.x() > 0 && params.b.y() > 0);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScript("document.getElementById('test2').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    view_->UpdateIME();
    msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateStatus::ID, msg->type());
    ViewHostMsg_ImeUpdateStatus::Read(msg, &params);
    EXPECT_EQ(params.a, IME_DISABLE);
  }
}

