// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include "base/logging.h"
#include "base/base_paths_linux.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/browser_toolbar_view_gtk.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace {

static GdkPixbuf* LoadThemeImage(const std::string& filename) {
  FilePath path;
  bool ok = PathService::Get(base::DIR_SOURCE_ROOT, &path);
  DCHECK(ok);
  path = path.Append("chrome/app/theme").Append(filename);
  // We intentionally ignore errors here, as some buttons don't have images
  // for all states.  This will all be removed once ResourceBundle works.
  return gdk_pixbuf_new_from_file(path.value().c_str(), NULL);
}

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

BrowserWindowGtk::BrowserWindowGtk(Browser* browser)
    :  browser_(browser),
       // TODO(port): make this a pref.
       custom_frame_(false) {
  Init();
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserWindowGtk::~BrowserWindowGtk() {
  browser_->tabstrip_model()->RemoveObserver(this);

  Close();
}

gboolean BrowserWindowGtk::OnContentAreaExpose(GtkWidget* widget,
                                               GdkEventExpose* e,
                                               BrowserWindowGtk* window) {
  if (window->custom_frame_) {
    NOTIMPLEMENTED() << " needs custom drawing for the custom frame.";
    return FALSE;
  }

  // The theme graphics include the 2px frame, but we don't draw the frame
  // in the non-custom-frame mode.  So we subtract it off.
  const int kFramePixels = 2;

  GdkPixbuf* pixbuf =
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, true,  // alpha
                     8,  // bit depth
                     widget->allocation.width,
                     BrowserToolbarGtk::kToolbarHeight + kFramePixels);

#ifndef NDEBUG
  // Fill with a bright color so we can see any pixels we're missing.
  gdk_pixbuf_fill(pixbuf, 0x00FFFFFF);
#endif

  window->content_area_ninebox_->RenderTopCenterStrip(pixbuf, 0,
                                                      widget->allocation.width);
  gdk_draw_pixbuf(widget->window, NULL, pixbuf,
                  0, 0,
                  widget->allocation.x,
                  widget->allocation.y - kFramePixels,
                  -1, -1,
                  GDK_RGB_DITHER_NORMAL, 0, 0);
  gdk_pixbuf_unref(pixbuf);

  return FALSE;  // Allow subwidgets to paint.
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

  GdkPixbuf* images[9] = {
    LoadThemeImage("content_top_left_corner.png"),
    LoadThemeImage("content_top_center.png"),
    LoadThemeImage("content_top_right_corner.png"),
    LoadThemeImage("content_left_side.png"),
    NULL,
    LoadThemeImage("content_right_side.png"),
    LoadThemeImage("content_bottom_left_corner.png"),
    LoadThemeImage("content_bottom_center.png"),
    LoadThemeImage("content_bottom_right_corner.png")
  };
  content_area_ninebox_.reset(new NineBox(images));

  // This vbox is intended to surround the "content": toolbar+page.
  // When we add the tab strip, it should go in a vbox surrounding this one.
  vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_app_paintable(vbox_, TRUE);
  gtk_widget_set_double_buffered(vbox_, FALSE);
  g_signal_connect(G_OBJECT(vbox_), "expose-event",
                   G_CALLBACK(&OnContentAreaExpose), this);

  toolbar_.reset(new BrowserToolbarGtk(browser_.get()));
  toolbar_->Init(browser_->profile());
  toolbar_->AddToolbarToBox(vbox_);

  contents_container_.reset(new TabContentsContainerGtk());
  contents_container_->AddContainerToBox(vbox_);

  // Note that calling this the first time is necessary to get the
  // proper control layout.
  // TODO(port): make this a pref.
  SetCustomFrame(false);

  status_bubble_.reset(new StatusBubbleGtk(window_));

  gtk_container_add(GTK_CONTAINER(window_), vbox_);
}

void BrowserWindowGtk::Show() {
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
  return status_bubble_.get();
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

bool BrowserWindowGtk::IsMaximized() const {
  return (state_ & GDK_WINDOW_STATE_MAXIMIZED);
}

void BrowserWindowGtk::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowGtk::SetFocusToLocationBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateStopGoState(bool is_loading) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateToolbar(TabContents* contents,
                                     bool should_restore_state) {
  toolbar_->UpdateTabContents(contents, should_restore_state);
}

void BrowserWindowGtk::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect BrowserWindowGtk::GetRootWindowResizerRect() const {
  return gfx::Rect();
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

void BrowserWindowGtk::TabDetachedAt(TabContents* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetSelectedTabContents() will return NULL or something else.
  if (index == browser_->tabstrip_model()->selected_index()) {
    // TODO(port): Uncoment this line when we get infobars.
    // infobar_container_->ChangeTabContents(NULL);
    contents_container_->SetTabContents(NULL);
  }
}

void BrowserWindowGtk::TabSelectedAt(TabContents* old_contents,
                                     TabContents* new_contents,
                                     int index,
                                     bool user_gesture) {
  DCHECK(old_contents != new_contents);

  // Update various elements that are interested in knowing the current
  // TabContents.
  // TOOD(port): Un-comment this line when we get infobars.
  // infobar_container_->ChangeTabContents(new_contents);
  contents_container_->SetTabContents(new_contents);

  new_contents->DidBecomeSelected();

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
}

void BrowserWindowGtk::TabStripEmpty() {
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

void BrowserWindowGtk::SetCustomFrame(bool custom_frame) {
  custom_frame_ = custom_frame;
  if (custom_frame_) {
    gtk_container_set_border_width(GTK_CONTAINER(vbox_), 2);
    // TODO(port): all the crazy blue title bar, etc.
    NOTIMPLEMENTED();
  } else {
    gtk_container_set_border_width(GTK_CONTAINER(vbox_), 0);
  }
}
