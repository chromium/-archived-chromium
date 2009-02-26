// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace {

// Callback used in WebContentsViewGtk::CreateViewForWidget().
void RemoveWidget(GtkWidget* widget, void* container) {
  gtk_container_remove(GTK_CONTAINER(container), widget);
}

}  // namespace

// static
WebContentsView* WebContentsView::Create(WebContents* web_contents) {
  return new WebContentsViewGtk(web_contents);
}

WebContentsViewGtk::WebContentsViewGtk(WebContents* web_contents)
    : web_contents_(web_contents),
      vbox_(gtk_vbox_new(FALSE, 0)) {
  g_object_ref_sink(vbox_);
}

WebContentsViewGtk::~WebContentsViewGtk() {
  gtk_widget_destroy(vbox_);
}

WebContents* WebContentsViewGtk::GetWebContents() {
  return web_contents_;
}

void WebContentsViewGtk::CreateView() {
  NOTIMPLEMENTED();
}

RenderWidgetHostView* WebContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  gtk_widget_show(view->native_view());
  gtk_container_foreach(GTK_CONTAINER(vbox_), RemoveWidget, vbox_);
  gtk_box_pack_start(GTK_BOX(vbox_), view->native_view(), TRUE, TRUE, 0);
  return view;
}

gfx::NativeView WebContentsViewGtk::GetNativeView() const {
  return vbox_;
}

gfx::NativeView WebContentsViewGtk::GetContentNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeWindow WebContentsViewGtk::GetTopLevelNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::OnContentsDestroy() {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::SetPageTitle(const std::wstring& title) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::Invalidate() {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::SizeContents(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::HideFindBar(bool end_session) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ReparentFindWindow(Browser* new_browser) const {
  NOTIMPLEMENTED();
}

bool WebContentsViewGtk::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::TakeFocus(bool reverse) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  // The windows code forwards this event onwards to accelerator handling,
  // and then to DefWindowProc.  TODO(port): should do something similar.
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();
}

WebContents* WebContentsViewGtk::CreateNewWindowInternal(
    int route_id,
    base::WaitableEvent* modal_dialog_event) {
  NOTIMPLEMENTED();
  return NULL;
}

RenderWidgetHostView* WebContentsViewGtk::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGtk::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}

