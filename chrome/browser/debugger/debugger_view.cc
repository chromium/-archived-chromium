// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_view.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "chrome/common/url_constants.h"
#include "grit/debugger_resources.h"
#include "views/grid_layout.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/controls/scroll_view.h"
#include "views/standard_layout.h"
#include "views/view.h"

DebuggerView::DebuggerView(DebuggerWindow* window)
    : window_(window), output_ready_(false) {
  web_container_ = new TabContentsContainer;
  AddChildView(web_container_);
  AddAccelerator(views::Accelerator(VK_ESCAPE, false, false, false));
}

DebuggerView::~DebuggerView() {
}

gfx::Size DebuggerView::GetPreferredSize() {
  return gfx::Size(700, 400);
}

void DebuggerView::Layout() {
  web_container_->SetBounds(0, 0, width(), height());
}


void DebuggerView::ViewHierarchyChanged(bool is_add,
                                        views::View* parent,
                                        views::View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    OnInit();
  }
}

void DebuggerView::Paint(gfx::Canvas* canvas) {
#ifndef NDEBUG
  SkPaint paint;
  canvas->FillRectInt(SK_ColorCYAN, x(), y(), width(), height());
#endif
}

void DebuggerView::SetOutputViewReady() {
  output_ready_ = true;
  for (std::vector<std::wstring>::iterator i = pending_output_.begin();
       i != pending_output_.end(); ++i) {
    Output(*i);
  }
  pending_output_.clear();

  for (std::vector<std::string>::const_iterator i = pending_events_.begin();
       i != pending_events_.end(); ++i) {
    ExecuteJavascript(*i);
  }
  pending_events_.clear();
}

void DebuggerView::Output(const std::string& out) {
  Output(UTF8ToWide(out));
}

void DebuggerView::Output(const std::wstring& out) {
  if (!output_ready_) {
    pending_output_.push_back(out);
    return;
  }

  DictionaryValue* body = new DictionaryValue;
  body->Set(L"text", Value::CreateStringValue(out));
  SendEventToPage(L"appendText", body);
}

void DebuggerView::OnInit() {
  // We can't create the TabContents until we've actually been put into a real
  // view hierarchy somewhere.
  Profile* profile = BrowserList::GetLastActive()->profile();
  tab_contents_ = new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL);

  tab_contents_->set_delegate(this);
  web_container_->ChangeTabContents(tab_contents_);
  tab_contents_->render_view_host()->AllowDOMUIBindings();

  GURL contents(std::string(chrome::kChromeUIScheme) +
                "://inspector/debugger.html");
  tab_contents_->controller().LoadURL(contents, GURL(),
                                      PageTransition::START_PAGE);
}

void DebuggerView::OnShow() {
  tab_contents_->Focus();
}

void DebuggerView::OnClose() {
  web_container_->ChangeTabContents(NULL);
  delete tab_contents_;
}

void DebuggerView::OpenURLFromTab(TabContents* source,
                               const GURL& url,
                               const GURL& referrer,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) {
  BrowserList::GetLastActive()->OpenURL(url, referrer, disposition,
                                        transition);
}


void DebuggerView::SendEventToPage(const std::wstring& name,
                                   Value* body) {
  DictionaryValue msg;
  msg.SetString(L"type", L"event");
  msg.SetString(L"event", name);
  msg.Set(L"body", body);

  std::string json;
  JSONWriter::Write(&msg, false, &json);

  const std::string js =
    StringPrintf("DebuggerIPC.onMessageReceived(%s)", json.c_str());
  if (output_ready_) {
    ExecuteJavascript(js);
  } else {
    pending_events_.push_back(js);
  }
}

void DebuggerView::ExecuteJavascript(const std::string& js) {
  tab_contents_->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      UTF8ToWide(js));
}

void DebuggerView::LoadingStateChanged(TabContents* source) {
  if (!source->is_loading())
    SetOutputViewReady();
}

bool DebuggerView::AcceleratorPressed(const views::Accelerator& accelerator) {
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);
  window_->window()->Close();
  return true;
}
