// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_view.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#if defined(OS_WIN)
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#endif
#include "views/widget/widget.h"

ExtensionView::ExtensionView(ExtensionHost* host, Browser* browser)
    : host_(host), browser_(browser),
      initialized_(false), pending_preferred_width_(0), container_(NULL),
      did_insert_css_(false), is_clipped_(false) {
  host_->set_view(this);
}

ExtensionView::~ExtensionView() {
  View* parent = GetParent();
  if (parent)
    parent->RemoveChildView(this);
  CleanUp();
}

Extension* ExtensionView::extension() const {
  return host_->extension();
}

RenderViewHost* ExtensionView::render_view_host() const {
  return host_->render_view_host();
}

void ExtensionView::SetDidInsertCSS(bool did_insert) {
  did_insert_css_ = did_insert;
  ShowIfCompletelyLoaded();
}

void ExtensionView::SetVisible(bool is_visible) {
  if (is_visible != IsVisible()) {
    NativeViewHost::SetVisible(is_visible);

    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View heirarchy and does not know about the change
    // unless we tell it.
    if (render_view_host()->view()) {
      if (is_visible)
        render_view_host()->view()->Show();
      else
        render_view_host()->view()->Hide();
    }
  }
}

void ExtensionView::DidChangeBounds(const gfx::Rect& previous,
                                    const gfx::Rect& current) {
  View::DidChangeBounds(previous, current);
  // Propagate the new size to RenderWidgetHostView.
  // We can't send size zero because RenderWidget DCHECKs that.
  if (render_view_host()->view() && !current.IsEmpty())
    render_view_host()->view()->SetSize(gfx::Size(width(), height()));
}

void ExtensionView::CreateWidgetHostView() {
  DCHECK(!initialized_);
  initialized_ = true;
  RenderWidgetHostView* view =
      RenderWidgetHostView::CreateViewForWidget(render_view_host());

  // TODO(mpcomplete): RWHV needs a cross-platform Init function.
#if defined(OS_WIN)
  // Create the HWND. Note:
  // RenderWidgetHostHWND supports windowed plugins, but if we ever also
  // wanted to support constrained windows with this, we would need an
  // additional HWND to parent off of because windowed plugin HWNDs cannot
  // exist in the same z-order as constrained windows.
  RenderWidgetHostViewWin* view_win =
      static_cast<RenderWidgetHostViewWin*>(view);
  HWND hwnd = view_win->Create(GetWidget()->GetNativeView());
  view_win->ShowWindow(SW_SHOW);
  Attach(hwnd);
#else
  NOTIMPLEMENTED();
#endif

  host_->CreateRenderView(view);
  SetVisible(false);

  if (!pending_background_.empty()) {
    render_view_host()->view()->SetBackground(pending_background_);
    pending_background_.reset();
  }
}

void ExtensionView::ShowIfCompletelyLoaded() {
  // We wait to show the ExtensionView until it has loaded, our parent has
  // given us a background and css has been inserted into page. These can happen
  // in different orders.
  if (!IsVisible() && host_->did_stop_loading() && render_view_host()->view() &&
      !is_clipped_ && did_insert_css_ &&
      !render_view_host()->view()->background().empty()) {
    SetVisible(true);
    DidContentsPreferredWidthChange(pending_preferred_width_);
  }
}

void ExtensionView::CleanUp() {
  if (!initialized_)
    return;
  if (native_view())
    Detach();
  initialized_ = false;
}

void ExtensionView::SetBackground(const SkBitmap& background) {
  if (initialized_ && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
  ShowIfCompletelyLoaded();
}

void ExtensionView::DidContentsPreferredWidthChange(const int pref_width) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!IsVisible()) {
    pending_preferred_width_ = pref_width;
  } else if (pref_width > 0 && pref_width != GetPreferredSize().width()) {
    SetPreferredSize(gfx::Size(pref_width, height()));
  }
}

void ExtensionView::ViewHierarchyChanged(bool is_add,
                                         views::View *parent,
                                         views::View *child) {
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !initialized_)
    CreateWidgetHostView();
}

void ExtensionView::RecoverCrashedExtension() {
  CleanUp();
  CreateWidgetHostView();
}

void ExtensionView::HandleMouseEvent() {
  if (container_)
    container_->OnExtensionMouseEvent(this);
}

void ExtensionView::HandleMouseLeave() {
  if (container_)
    container_->OnExtensionMouseLeave(this);
}
