// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_

#include "base/base_drag_source.h"
#include "base/basictypes.h"

class RenderViewHost;

///////////////////////////////////////////////////////////////////////////////
//
// WebDragSource
//
//  An IDropSource implementation for a WebContents. Handles notifications sent
//  by an active drag-drop operation as the user mouses over other drop targets
//  on their system. This object tells Windows whether or not the drag should
//  continue, and supplies the appropriate cursors.
//
class WebDragSource : public BaseDragSource {
 public:
  // Create a new DragSource for a given HWND and RenderViewHost.
  WebDragSource(HWND source_hwnd, RenderViewHost* render_view_host);
  virtual ~WebDragSource() { }

 protected:
  // BaseDragSource
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  // Cannot construct thusly.
  WebDragSource();

  // Keep a reference to the HWND so we can translate the cursor position.
  HWND source_hwnd_;

  // We use this as a channel to the renderer to tell it about various drag
  // drop events that it needs to know about (such as when a drag operation it
  // initiated terminates).
  RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSource);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_

