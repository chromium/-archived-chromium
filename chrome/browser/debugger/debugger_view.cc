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

#include "chrome/browser/debugger/debugger_window.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/debugger/debugger_view.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_container_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_scroll_bar.h"
#include "chrome/views/scroll_view.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view.h"


DebuggerView::DebuggerView(ChromeViews::TextField::Controller* controller) {
  command_text_ = new ChromeViews::TextField();
  command_text_->SetFont(font_);
  command_text_->SetController(controller);
  AddChildView(command_text_);
  web_container_ = new TabContentsContainerView();
  AddChildView(web_container_);
}

DebuggerView::~DebuggerView() {
}

void DebuggerView::GetPreferredSize(CSize* out) {
  out->cx = 700;
  out->cy = 400;
}

void DebuggerView::Layout() {
  int cmd_height = 20;
  web_container_->SetBounds(0, 0, GetWidth(), GetHeight() - cmd_height);
  command_text_->SetBounds(0, GetHeight() - cmd_height, GetWidth(), cmd_height);
}

void DebuggerView::Paint(ChromeCanvas* canvas) {
#ifndef NDEBUG
  SkPaint paint;
  canvas->FillRectInt(SK_ColorCYAN, bounds_.left, bounds_.top,
                      bounds_.Width(), bounds_.Height());
#endif
}

void DebuggerView::Output(const std::string& out) {
  Output(UTF8ToWide(out));
}

void DebuggerView::Output(const std::wstring& out) {
  if (web_contents_->is_loading()) {
    Sleep(100);
  }
  Value* str_value = Value::CreateStringValue(out);
  std::string json;
  JSONWriter::Write(str_value, false, &json);
  const std::string js =
      StringPrintf("javascript:void(appendText(%s))", json.c_str());
  web_contents_->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      UTF8ToWide(js));
}

void DebuggerView::OnInit() {
  // We can't create the WebContents until we've actually been put into a real
  // view hierarchy somewhere.
  Profile* profile = BrowserList::GetLastActive()->profile();
  TabContents* tc = TabContents::CreateWithType(TAB_CONTENTS_WEB,
      ::GetDesktopWindow(), profile, NULL);
  web_contents_ = tc->AsWebContents();
  web_contents_->SetupController(profile);
  web_contents_->set_delegate(this);
  web_container_->SetTabContents(web_contents_);

  // TODO(erikkay): move this into chrome-tools scheme when that gets added.
  // This will allow us to do some spiffier things as well as making this
  // HTML easier to maintain.
  GURL contents("data:text/html,<html><head><script>function appendText(txt){var output = document.getElementById('output');  output.appendChild(document.createTextNode(txt));  output.appendChild(document.createElement('br'));  document.body.scrollTop = document.body.scrollHeight;};</script><style type='text/css'>body{margin:0px;padding:0px;}#output {  font-family: monospace; background-} #outer { width: 100%;  height: 100%;  white-space: pre-wrap;}</style></head><body><table id='outer'><tr><td valign='bottom' id='output'>JavaScript Debugger<br/></td></tr></table></body></html>");
  web_contents_->controller()->LoadURL(contents, PageTransition::START_PAGE);
}

void DebuggerView::OnShow() {
  command_text_->RequestFocus();
}

void DebuggerView::OnClose() {
  web_container_->SetTabContents(NULL);

  web_contents_->CloseContents();
}

void DebuggerView::OpenURLFromTab(TabContents* source,
                               const GURL& url,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) {
  BrowserList::GetLastActive()->OpenURL(url, disposition, transition);
}
