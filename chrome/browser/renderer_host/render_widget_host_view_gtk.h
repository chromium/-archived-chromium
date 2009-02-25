// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "webkit/glue/webcursor.h"

class RenderWidgetHost;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewGtk : public RenderWidgetHostView {
 public:
  RenderWidgetHostViewGtk(RenderWidgetHost* widget);
  ~RenderWidgetHostViewGtk();

  // ---------------------------------------------------------------------------
  // Implementation of RenderWidgetHostView...

  RenderWidgetHost* GetRenderWidgetHost() const { return host_; }
  void DidBecomeSelected();
  void WasHidden();
  void SetSize(const gfx::Size& size);
  gfx::NativeView GetPluginNativeView();
  void MovePluginWindows(
      const std::vector<WebPluginGeometry>& plugin_window_moves);
  void Focus();
  void Blur();
  bool HasFocus();
  void Show();
  void Hide();
  gfx::Rect GetViewBounds() const;
  void UpdateCursor(const WebCursor& cursor);
  void UpdateCursorIfOverSelf();
  void SetIsLoading(bool is_loading);
  void IMEUpdateStatus(int control, const gfx::Rect& caret_rect);
  void DidPaintRect(const gfx::Rect& rect);
  void DidScrollRect(
      const gfx::Rect& rect, int dx, int dy);
  void RenderViewGone();
  void Destroy();
  void SetTooltipText(const std::wstring& tooltip_text);
  // ---------------------------------------------------------------------------

  gfx::NativeView native_view() const { return view_; }

  void Paint(const gfx::Rect&);

 private:
  // The model object.
  RenderWidgetHost *const host_;
  // The native UI widget.
  gfx::NativeView view_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
