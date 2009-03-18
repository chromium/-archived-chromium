// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/owned_widget_gtk.h"
#include "webkit/glue/webcursor.h"

class RenderWidgetHost;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewGtk : public RenderWidgetHostView {
 public:
  RenderWidgetHostViewGtk(RenderWidgetHost* widget);
  ~RenderWidgetHostViewGtk();

  // Initialize this object for use as a drawing area.
  void InitAsChild();

  // Initialize this object for use as a popup (e.g. HTML dropdown menu).
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos);

  // TODO(estade): unfork this with RenderWidgetHostViewWin function of same
  // name.
  void set_activatable(bool activatable) { activatable_ = activatable; }

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
  BackingStore* AllocBackingStore(const gfx::Size& size);
  // ---------------------------------------------------------------------------

  gfx::NativeView native_view() const { return view_.get(); }

  void Paint(const gfx::Rect&);

 private:
  // The model object.
  RenderWidgetHost *const host_;
  // The native UI widget.
  OwnedWidgetGtk view_;
  // Variables used only for popups --------------------------------------------
  // Our parent widget.
  RenderWidgetHostView* parent_host_view_;
  // The native view of our parent, equivalent to
  // parent_host_view_->GetPluginNativeView().
  GtkWidget* parent_;
  // We connect to the parent's focus out event. When we are destroyed, we need
  // to remove this handler, so we must keep track of its id.
  gulong popup_signal_id_;
  // This variable determines our degree of control over user input. If we are
  // activatable, we must grab and handle all user input. If we are not
  // activatable, then our parent render view retains more control. Example of
  // activatable popup: <select> dropdown. Example of non-activatable popup:
  // form autocomplete.
  bool activatable_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
