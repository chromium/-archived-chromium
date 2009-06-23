// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_

#include <gdk/gdk.h>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/owned_widget_gtk.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/webcursor.h"

class RenderWidgetHost;

typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkSelectionData GtkSelectionData;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewGtk : public RenderWidgetHostView {
 public:
  RenderWidgetHostViewGtk(RenderWidgetHost* widget);
  ~RenderWidgetHostViewGtk();

  // Initialize this object for use as a drawing area.
  void InitAsChild();

  // RenderWidgetHostView implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos);
  virtual RenderWidgetHost* GetRenderWidgetHost() const { return host_; }
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void SetSize(const gfx::Size& size);
  virtual gfx::NativeView GetNativeView();
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& plugin_window_moves);
  virtual void Focus();
  virtual void Blur();
  virtual bool HasFocus();
  virtual void Show();
  virtual void Hide();
  virtual gfx::Rect GetViewBounds() const;
  virtual void UpdateCursor(const WebCursor& cursor);
  virtual void SetIsLoading(bool is_loading);
  virtual void IMEUpdateStatus(int control, const gfx::Rect& caret_rect);
  virtual void DidPaintRect(const gfx::Rect& rect);
  virtual void DidScrollRect(const gfx::Rect& rect, int dx, int dy);
  virtual void RenderViewGone();
  virtual void Destroy();
  virtual void SetTooltipText(const std::wstring& tooltip_text);
  virtual void SelectionChanged(const std::string& text);
  virtual void PasteFromSelectionClipboard();
  virtual void ShowingContextMenu(bool showing);
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);

  gfx::NativeView native_view() const { return view_.get(); }

  void Paint(const gfx::Rect&);

 private:
  friend class RenderWidgetHostViewGtkWidget;

  // Update the display cursor for the render view.
  void ShowCurrentCursor();

  // When we've requested the text from the X clipboard, GTK returns it to us
  // through this callback.
  static void ReceivedSelectionText(GtkClipboard* clipboard,
                                    const gchar* text,
                                    gpointer userdata);

  // The model object.
  RenderWidgetHost* const host_;

  // The native UI widget.
  OwnedWidgetGtk view_;

  // This is true when we are currently painting and thus should handle extra
  // paint requests by expanding the invalid rect rather than actually
  // painting.
  bool about_to_validate_and_paint_;

  // This is the rectangle which we'll paint.
  gfx::Rect invalid_rect_;

  // Whether or not this widget is hidden.
  bool is_hidden_;

  // Whether we are currently loading.
  bool is_loading_;
  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Whether we are showing a context menu.
  bool is_showing_context_menu_;

  // Variables used only for popups --------------------------------------------
  // Our parent widget.
  RenderWidgetHostView* parent_host_view_;
  // The native view of our parent, equivalent to
  // parent_host_view_->GetNativeView().
  GtkWidget* parent_;
  // We ignore the first mouse release on popups.  This allows the popup to
  // stay open.
  bool is_popup_first_mouse_release_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
