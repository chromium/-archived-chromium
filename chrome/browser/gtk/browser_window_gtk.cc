// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_toolbar_view_gtk.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace {

gboolean MainWindowDestroyed(GtkWindow* window, BrowserWindowGtk* browser_win) {
  delete browser_win;
  return FALSE;  // Don't stop this message.
}

gboolean MainWindowConfigured(GtkWindow* window, GdkEventConfigure* event,
                              BrowserWindowGtk* browser_win) {
  gfx::Rect bounds = gfx::Rect(event->x, event->y, event->width, event->height);
  browser_win->OnBoundsChanged(bounds);
  return FALSE;
}

gboolean MainWindowStateChanged(GtkWindow* window, GdkEventWindowState* event,
                                BrowserWindowGtk* browser_win) {
  browser_win->OnStateChanged(event->new_window_state);
  return FALSE;
}

// Using gtk_window_get_position/size creates a race condition, so only use
// this to get the initial bounds.  After window creation, we pick up the
// normal bounds by connecting to the configure-event signal.
gfx::Rect GetInitialWindowBounds(GtkWindow* window) {
  gint x, y, width, height;
  gtk_window_get_position(window, &x, &y);
  gtk_window_get_size(window, &width, &height);
  return gfx::Rect(x, y, width, height);
}

}  // namespace

BrowserWindowGtk::BrowserWindowGtk(Browser* browser) : browser_(browser) {
  Init();
}

BrowserWindowGtk::~BrowserWindowGtk() {
  Close();
}

void BrowserWindowGtk::Init() {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window_, "Chromium");
  gtk_window_set_default_size(window_, 640, 480);
  g_signal_connect(G_OBJECT(window_), "destroy",
                   G_CALLBACK(MainWindowDestroyed), this);
  g_signal_connect(G_OBJECT(window_), "configure-event",
                   G_CALLBACK(MainWindowConfigured), this);
  g_signal_connect(G_OBJECT(window_), "window-state-event",
                   G_CALLBACK(MainWindowStateChanged), this);
  bounds_ = GetInitialWindowBounds(window_);

  vbox_ = gtk_vbox_new(FALSE, 0);

  toolbar_.reset(new BrowserToolbarGtk(browser_.get()));
  toolbar_->Init(browser_->profile());
  toolbar_->AddToolbarToBox(vbox_);

  gtk_container_add(GTK_CONTAINER(window_), vbox_);
}

void BrowserWindowGtk::Show() {
  // TODO(estade): fix this block. As it stands, it is a temporary hack to get
  // the browser displaying something.
  if (content_area_ == NULL) {
    WebContents* contents = (WebContents*)(browser_->GetTabContentsAt(0));
    content_area_ = ((RenderWidgetHostViewGtk*)contents->
        render_view_host()->view())->native_view();
    gtk_box_pack_start(GTK_BOX(vbox_), content_area_, TRUE, TRUE, 0);
  }

  gtk_widget_show_all(GTK_WIDGET(window_));
}

void BrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  gint x = static_cast<gint>(bounds.x());
  gint y = static_cast<gint>(bounds.y());
  gint width = static_cast<gint>(bounds.width());
  gint height = static_cast<gint>(bounds.height());

  gtk_window_move(window_, x, y);
  gtk_window_resize(window_, width, height);
}

void BrowserWindowGtk::Close() {
  if (!window_)
    return;

  gtk_widget_destroy(GTK_WIDGET(window_));
  window_ = NULL;
}

void BrowserWindowGtk::Activate() {
  gtk_window_present(window_);
}

void BrowserWindowGtk::FlashFrame() {
  // May not be respected by all window managers.
  gtk_window_set_urgency_hint(window_, TRUE);
}

void* BrowserWindowGtk::GetNativeHandle() {
  return window_;
}

BrowserWindowTesting* BrowserWindowGtk::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* BrowserWindowGtk::GetStatusBubble() {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowGtk::SelectedTabToolbarSizeChanged(bool is_animating) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateTitleBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateLoadingAnimations(bool should_animate) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::SetStarredState(bool is_starred) {
  NOTIMPLEMENTED();
}

gfx::Rect BrowserWindowGtk::GetNormalBounds() const {
  return bounds_;
}

bool BrowserWindowGtk::IsMaximized() {
  return (state_ & GDK_WINDOW_STATE_MAXIMIZED);
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowGtk::UpdateStopGoState(bool is_loading) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateToolbar(TabContents* contents,
                                     bool should_restore_state) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void BrowserWindowGtk::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowBookmarkManager() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void BrowserWindowGtk::ShowBookmarkBubble(const GURL& url,
                                          bool already_bookmarked) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowImportDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowSearchEnginesDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                      void* parent_window) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::DestroyBrowser() {
  browser_.reset();
}

void BrowserWindowGtk::OnBoundsChanged(const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void BrowserWindowGtk::OnStateChanged(GdkWindowState state) {
  state_ = state;
}
