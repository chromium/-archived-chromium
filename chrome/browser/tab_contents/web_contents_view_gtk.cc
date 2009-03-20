// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"

#include "webkit/glue/webinputevent.h"

namespace {

// Called when the content view gtk widget is tabbed to. We always return true
// and grab focus if we don't have it. The call to SetInitialFocus(bool)
// forwards the tab to webkit. We leave focus via TakeFocus().
// We cast the WebContents to a TabContents because SetInitialFocus is public
// in TabContents and protected in WebContents.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  if (GTK_WIDGET_HAS_FOCUS(widget))
    return TRUE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->SetInitialFocus(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       WebContents* web_contents) {
  if (web_contents->delegate())
    web_contents->delegate()->ContentsMouseEvent(web_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     WebContents* web_contents) {
  if (web_contents->delegate())
    web_contents->delegate()->ContentsMouseEvent(web_contents, true);
  return FALSE;
}

// Callback used in WebContentsViewGtk::CreateViewForWidget().
void RemoveWidget(GtkWidget* widget, gpointer container) {
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
}

WebContentsViewGtk::~WebContentsViewGtk() {
  vbox_.Destroy();
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
  view->InitAsChild();
  content_view_ = view->native_view();
  g_signal_connect(content_view_, "focus",
                   G_CALLBACK(OnFocus), web_contents_);
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), web_contents_);
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(OnMouseMove), web_contents_);
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  gtk_container_foreach(GTK_CONTAINER(vbox_.get()), RemoveWidget, vbox_.get());
  gtk_box_pack_start(GTK_BOX(vbox_.get()), content_view_, TRUE, TRUE, 0);
  return view;
}

gfx::NativeView WebContentsViewGtk::GetNativeView() const {
  return vbox_.get();
}

gfx::NativeView WebContentsViewGtk::GetContentNativeView() const {
  return content_view_;
}

gfx::NativeWindow WebContentsViewGtk::GetTopLevelNativeWindow() const {
  return GTK_WINDOW(gtk_widget_get_toplevel(vbox_.get()));
}

void WebContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::OnContentsDestroy() {
  // TODO(estade): Windows uses this function cancel pending drag-n-drop drags.
  // We don't have drags yet, so do nothing for now.
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

void WebContentsViewGtk::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->delegate()->SetFocusToLocationBar();
  else
    gtk_widget_grab_focus(content_view_);
}

void WebContentsViewGtk::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::RestoreFocus() {
  // TODO(estade): implement this function.
  // For now just assume we are viewing the tab for the first time.
  SetInitialFocus();
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewGtk::TakeFocus(bool reverse) {
  web_contents_->delegate()->SetFocusToLocationBar();
}

void WebContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // This may be an accelerator. Pass it on to GTK.
  GtkWindow* window = GetTopLevelNativeWindow();
  gtk_accel_groups_activate(G_OBJECT(window), event.os_event->keyval,
                            GdkModifierType(event.os_event->state));
}

void WebContentsViewGtk::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  NOTIMPLEMENTED();
}

void WebContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(web_contents_, params));
  context_menu_->Popup();
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

// TODO(estade): unfork this from the version in web_contents_view_win.
RenderWidgetHostView* WebContentsViewGtk::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(web_contents_->process(), route_id);
  RenderWidgetHostViewGtk* widget_view =
      new RenderWidgetHostViewGtk(widget_host);
  widget_view->set_activatable(activatable);

  return widget_view;
}

void WebContentsViewGtk::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  NOTIMPLEMENTED();
}

// TODO(estade): unfork this from the version in web_contents_view_win.
void WebContentsViewGtk::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  RenderWidgetHostViewGtk* widget_host_view_gtk =
      static_cast<RenderWidgetHostViewGtk*>(widget_host_view);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  widget_host_view_gtk->InitAsPopup(
      web_contents_->render_widget_host_view(), initial_pos);
}
