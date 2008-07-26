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

#ifndef CHROME_BROWSER_WEB_DROP_TARGET_H__
#define CHROME_BROWSER_WEB_DROP_TARGET_H__

#include "base/base_drop_target.h"
#include "base/scoped_ptr.h"

class InterstitialDropTarget;
class WebContents;

///////////////////////////////////////////////////////////////////////////////
//
// WebDropTarget
//
//  A helper object that provides drop capabilities to a WebContents. The
//  DropTarget handles drags that enter the region of the WebContents by
//  passing on the events to the renderer.
//
class WebDropTarget : public BaseDropTarget {
 public:
  // Create a new WebDropTarget associating it with the given HWND and
  // WebContents.
  WebDropTarget(HWND source_hwnd, WebContents* contents);
  virtual ~WebDropTarget();

  void set_is_drop_target(bool is_drop_target) {
    is_drop_target_ = is_drop_target;
  }

 protected:
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
  // Our associated WebContents.
  WebContents* web_contents_;

  // A special drop target handler for when we try to d&d while an interstitial
  // page is showing.
  scoped_ptr<InterstitialDropTarget> interstitial_drop_target_;

  // Used to determine what cursor we should display when dragging over web
  // content area.  This can be updated async during a drag operation.
  bool is_drop_target_;

  DISALLOW_EVIL_CONSTRUCTORS(WebDropTarget);
};

#endif  // #ifndef CHROME_BROWSER_WEB_DROP_TARGET_H__
