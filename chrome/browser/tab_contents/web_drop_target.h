// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_H_

#include "base/base_drop_target.h"
#include "base/scoped_ptr.h"

class InterstitialDropTarget;
class RenderViewHost;
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

  // We keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.  WARNING:
  // this pointer should never be dereferenced.  We only use it for comparing
  // pointers.
  RenderViewHost* current_rvh_;

  // Used to determine what cursor we should display when dragging over web
  // content area.  This can be updated async during a drag operation.
  bool is_drop_target_;

  // A special drop target handler for when we try to d&d while an interstitial
  // page is showing.
  scoped_ptr<InterstitialDropTarget> interstitial_drop_target_;

  DISALLOW_EVIL_CONSTRUCTORS(WebDropTarget);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_H_

