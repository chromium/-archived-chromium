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

#ifndef CHROME_BROWSER_WEB_DRAG_SOURCE_H__
#define CHROME_BROWSER_WEB_DRAG_SOURCE_H__

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

  DISALLOW_EVIL_CONSTRUCTORS(WebDragSource);
};

#endif  // #ifndef CHROME_BROWSER_WEB_DRAG_SOURCE_H__
