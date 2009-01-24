// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_

#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace gfx {
class Rect;
class Size;
}
namespace IPC {
class Message;
}

class RenderProcessHost;
class RenderWidgetHost;
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
  // Platform-specific creator. Use this to construct new RenderWidgetHostViews
  // rather than using RenderWidgetHostViewWin & friends.
  //
  // The RenderWidgetHost must already be created (because we can't know if it's
  // going to be a regular RenderWidgetHost or a RenderViewHost (a subclass).
  static RenderWidgetHostView* CreateViewForWidget(RenderWidgetHost* widget);

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Notifies the View that it has become visible.
  virtual void DidBecomeSelected() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

#if defined(OS_WIN)
  // Retrieves the HWND used to contain plugin HWNDs.
  virtual HWND GetPluginHWND() = 0;
#endif

  // Moves all plugin windows as described in the given list.
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& plugin_window_moves) = 0;

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
                               const gfx::Rect& caret_rect) = 0;

  // Informs the view that a portion of the widget's backing store was painted.
  // The view should copy the given rect from the backing store of the render
  // widget onto the screen.
  virtual void DidPaintRect(const gfx::Rect& rect) = 0;

  // Informs the view that a portion of the widget's backing store was scrolled
  // by dx pixels horizontally and dy pixels vertically. The view should copy
  // the exposed pixels from the backing store of the render widget (which has
  // already been scrolled) onto the screen.
  virtual void DidScrollRect(
      const gfx::Rect& rect, int dx, int dy) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RendererGone() = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const std::wstring& tooltip_text) = 0;

 protected:
  // Interface class only, do not construct.
  RenderWidgetHostView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostView);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_

