// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that implements BaseDragSource for the test shell webview delegate.

#ifndef WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__
#define WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__

#include "base/base_drag_source.h"

class WebView;

class TestDragDelegate : public BaseDragSource {
 public:
  TestDragDelegate(HWND source_hwnd, WebView* webview)
      : BaseDragSource(),
        source_hwnd_(source_hwnd),
        webview_(webview) { }

 protected:
  // BaseDragSource
  virtual void OnDragSourceCancel();
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  WebView* webview_;

  // A HWND for the source we are associated with, used for translating
  // mouse coordinates from screen to client coordinates.
  HWND source_hwnd_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__
