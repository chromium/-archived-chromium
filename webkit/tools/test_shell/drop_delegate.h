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
