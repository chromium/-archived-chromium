// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"

// TODO(port): Port this file.
#if defined(OS_WIN)
#include "base/base_drag_source.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

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
  WebDragSource(gfx::NativeWindow source_wnd, RenderViewHost* render_view_host);
  virtual ~WebDragSource() { }

 protected:
  // BaseDragSource
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  // Cannot construct thusly.
  WebDragSource();

  // Keep a reference to the window so we can translate the cursor position.
  gfx::NativeWindow source_wnd_;

  // We use this as a channel to the renderer to tell it about various drag
  // drop events that it needs to know about (such as when a drag operation it
  // initiated terminates).
  RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSource);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_H_

