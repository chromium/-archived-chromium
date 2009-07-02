// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "build/build_config.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/browser/views/tab_contents/render_view_context_menu_win.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/root_view.h"

using WebKit::WebInputEvent;


namespace {

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, true);
  return FALSE;
}

// See tab_contents_view_win.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      tab_contents->delegate()->ContentsZoomChange(false);
      return TRUE;
    } else if (event->direction == GDK_SCROLL_UP) {
      tab_contents->delegate()->ContentsZoomChange(true);
      return TRUE;
    }
  }

  return FALSE;
}

}  // namespace

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      views::WidgetGtk(TYPE_CHILD),
      ignore_next_char_event_(false) {
}

TabContentsViewGtk::~TabContentsViewGtk() {
}

void TabContentsViewGtk::CreateView() {
  set_delete_on_destroy(false);
  WidgetGtk::Init(NULL, gfx::Rect());
}

RenderWidgetHostView* TabContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), tab_contents());
/*
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
*/
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);

  gtk_fixed_put(GTK_FIXED(GetNativeView()), view->native_view(), 0, 0);
  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return WidgetGtk::GetNativeView();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}

gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  GetBounds(out, false);
}

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();

  // Until we have d'n'd implemented, just immediately pretend we're
  // already done with the drag and drop so we don't get stuck
  // thinking we're in mid-drag.
  // TODO(port): remove me when the above NOTIMPLEMENTED is fixed.
  if (tab_contents()->render_view_host())
    tab_contents()->render_view_host()->DragSourceSystemDragEnded();
}

void TabContentsViewGtk::OnContentsDestroy() {
  // TODO(brettw) this seems like maybe it can be moved into OnDestroy and this
  // function can be deleted? If you're adding more here, consider whether it
  // can be moved into OnDestroy which is a Windows message handler as the
  // window is being torn down.

  // When a tab is closed all its child plugin windows are destroyed
  // automatically. This happens before plugins get any notification that its
  // instances are tearing down.
  //
  // Plugins like Quicktime assume that their windows will remain valid as long
  // as they have plugin instances active. Quicktime crashes in this case
  // because its windowing code cleans up an internal data structure that the
  // handler for NPP_DestroyStream relies on.
  //
  // The fix is to detach plugin windows from web contents when it is going
  // away. This will prevent the plugin windows from getting destroyed
  // automatically. The detached plugin windows will get cleaned up in proper
  // sequence as part of the usual cleanup when the plugin instance goes away.
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed() {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.
  WasSized(size);
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
    return;
  }

  if (sad_tab_.get()) {
    sad_tab_->RequestFocus();
    return;
  }

  RenderWidgetHostView* rwhv = tab_contents()->render_widget_host_view();
  gtk_widget_grab_focus(rwhv ? rwhv->GetNativeView() : GetNativeView());
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::RestoreFocus() {
  NOTIMPLEMENTED();
  SetInitialFocus();
}

void TabContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::GotFocus() {
  tab_contents()->delegate()->TabContentsFocused(tab_contents());
}

void TabContentsViewGtk::TakeFocus(bool reverse) {
  // This is called when we the renderer asks us to take focus back (i.e., it
  // has iterated past the last focusable element on the page).
  gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
      reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

void TabContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
  // TODO(port): could be an accelerator, pass to parent.
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (tab_contents()->delegate()->HandleContextMenu(params))
    return;

  context_menu_.reset(new RenderViewContextMenuWin(tab_contents(), params));
  context_menu_->Init();

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(GetRootView(), &screen_point);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  context_menu_->RunMenuAt(screen_point.x(), screen_point.y());
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation) {
  WasSized(gfx::Size(allocation->width, allocation->height));
}

void TabContentsViewGtk::WasHidden() {
  tab_contents()->HideContents();
}

void TabContentsViewGtk::WasShown() {
  tab_contents()->ShowContents();
}

void TabContentsViewGtk::WasSized(const gfx::Size& size) {
  if (tab_contents()->interstitial_page())
    tab_contents()->interstitial_page()->SetSize(size);
  if (tab_contents()->render_widget_host_view())
    tab_contents()->render_widget_host_view()->SetSize(size);

  // TODO(brettw) this function can probably be moved to this class.
  tab_contents()->RepositionSupressedPopupsToFit();
}

// TODO(port): port BlockedPopupContainerViewWin...

class BlockedPopupContainerViewGtk : public BlockedPopupContainerView {
 public:
  BlockedPopupContainerViewGtk() {}
  virtual ~BlockedPopupContainerViewGtk() {}

  // Overridden from BlockedPopupContainerView:
  virtual void SetPosition() {}
  virtual void ShowView() {}
  virtual void UpdateLabel() {}
  virtual void HideView() {}
  virtual void Destroy() {
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewGtk);
};

// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewGtk;
}
