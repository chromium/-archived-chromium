// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that implements BaseDropTarget for the test shell webview delegate.

#ifndef WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H__
#define WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H__

#include "base/base_drop_target.h"

class WebView;

class TestDropDelegate : public BaseDropTarget {
 public:
  TestDropDelegate(HWND source_hwnd, WebView* webview)
      : BaseDropTarget(source_hwnd),
        webview_(webview) { }

 protected:
    // BaseDropTarget methods
    virtual DWORD OnDragEnter(IDataObject* data_object,
                              DWORD key_state,
                              POINT cursor_position,
                              DWORD effect);
    virtual DWORD OnDragOver(IDataObject* data_object,
                             DWORD key_state,
                             POINT cursor_position,
                             DWORD effect);
    virtual void OnDragLeave(IDataObject* data_object);
    virtual DWORD OnDrop(IDataObject* data_object,
                         DWORD key_state,
                         POINT cursor_position,
                         DWORD effect);


 private:
  WebView* webview_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H__
