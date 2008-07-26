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

#ifndef CHROME_BROWSER_RENDER_WIDGET_HOST_VIEW_H__
#define CHROME_BROWSER_RENDER_WIDGET_HOST_VIEW_H__

#include <windows.h>

#include "base/shared_memory.h"
#include "chrome/common/render_messages.h"

namespace gfx {
class Rect;
class Size;
}
namespace IPC {
class Message;
}

class WebCursor;

///////////////////////////////////////////////////////////////////////////////
//
// RenderWidgetHostView
//
//  RenderWidgetHostView is an interface implemented by an object that acts as
//  the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
//  associated RenderProcessHost own the "Model" in this case which is the
//  child renderer process. The View is responsible for receiving events from
//  the surrounding environment and passing them to the RenderWidgetHost, and
//  for actually displaying the content of the RenderWidgetHost when it
//  changes.
//
///////////////////////////////////////////////////////////////////////////////
class RenderWidgetHostView {
 public:
  // Notifies the View that it has become visible.
  virtual void DidBecomeSelected() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Retrieves the HWND used to contain plugin HWNDs.
  virtual HWND GetPluginHWND() = 0;

  // Sends the specified mouse event to the renderer.
  virtual void ForwardMouseEventToRenderer(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) = 0;

  // Actually set/take focus to/from the associated View component.
  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Returns true if the View currently has the focus.
  virtual bool HasFocus() = 0;

  // Shows/hides the view.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() const = 0;

  // Sets the cursor to the one associated with the specified cursor_type
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Updates the displayed cursor to the current one.
  virtual void UpdateCursorIfOverSelf() = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Enable or disable IME for the view.
  virtual void IMEUpdateStatus(ViewHostMsg_ImeControl control,
                               int x,
                               int y) = 0;

  // Informs the view that a portion of the widget's backing store was painted.
  virtual void DidPaintRect(const gfx::Rect& rect) = 0;

  // Informs the view that a portion of the widget's backing store was scrolled
  // by dx pixels horizontally and dy pixels vertically.
  virtual void DidScrollRect(
      const gfx::Rect& rect, int dx, int dy) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RendererGone() = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const std::wstring& tooltip_text) = 0;
};

#endif  // #ifndef CHROME_BROWSER_RENDER_WIDGET_HOST_VIEW_H__
