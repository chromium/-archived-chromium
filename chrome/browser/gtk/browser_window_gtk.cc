// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/base_paths_linux.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/find_bar_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/resource_bundle.h"
#include "grit/theme_resources.h"

namespace {

static GdkPixbuf* LoadThemeImage(int resource_id) {
  // TODO(mmoss) refactor -- stolen from custom_button.cc
  if (0 == resource_id)
    return NULL;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::vector<unsigned char> data;
  rb.LoadImageResourceBytes(resource_id, &data);

  GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
  bool ok = gdk_pixbuf_loader_write(loader, static_cast<guint8*>(data.data()),
      data.size(), NULL);
  DCHECK(ok) << "failed to write " << resource_id;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader, NULL);
  DCHECK(ok) << "close failed " << resource_id;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
  DCHECK(pixbuf) << "failed to load " << resource_id << " " << data.size();

  // The pixbuf is owned by the loader, so add a ref so when we delete the
  // loader, the pixbuf still exists.
  g_object_ref(pixbuf);
  g_object_unref(loader);

  return pixbuf;
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

const guint kFocusLocationKey = GDK_l;
const guint kFocusSearchKey = GDK_k;
const guint kOpenFileKey = GDK_o;

static int GetCommandFromKeyval(guint accel_key) {
  switch (accel_key) {
    case kFocusLocationKey:
      return IDC_FOCUS_LOCATION;
    case kFocusSearchKey:
      return IDC_FOCUS_SEARCH;
    case kOpenFileKey:
      return IDC_OPEN_FILE;
    default:
      NOTREACHED();
      return 0;
  }
}

// Usually accelerators are checked before propagating the key event, but in our
// case we want to reverse the order of things to allow webkit to handle key
// events like ctrl-l. If the window's children can handle the key event, this
// will return TRUE and the signal won't be propagated further. If the window's
// children cannot handle the key event then this will return FALSE and the
// default GtkWindow key press handler will be invoked.
gboolean OnKeyPress(GtkWindow* window, GdkEventKey* event, gpointer userdata) {
  return gtk_window_propagate_key_event(window, event);
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
  gtk_window_set_default_size(window_, 640, 480);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(OnWindowDestroyed), this);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(MainWindowConfigured), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(MainWindowStateChanged), this);
  g_signal_connect(window_, "key-press-event",
                   G_CALLBACK(OnKeyPress), NULL);
  ConnectAccelerators();
  bounds_ = GetInitialWindowBounds(window_);

  GdkPixbuf* images[9] = {
    LoadThemeImage(IDR_CONTENT_TOP_LEFT_CORNER),
    LoadThemeImage(IDR_CONTENT_TOP_CENTER),
    LoadThemeImage(IDR_CONTENT_TOP_RIGHT_CORNER),
    LoadThemeImage(IDR_CONTENT_LEFT_SIDE),
    NULL,
    LoadThemeImage(IDR_CONTENT_RIGHT_SIDE),
    LoadThemeImage(IDR_CONTENT_BOTTOM_LEFT_CORNER),
    LoadThemeImage(IDR_CONTENT_BOTTOM_CENTER),
    LoadThemeImage(IDR_CONTENT_BOTTOM_RIGHT_CORNER)
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
  toolbar_->Init(browser_->profile(), window_);
  toolbar_->AddToolbarToBox(vbox_);

  FindBarGtk* find_bar_gtk = new FindBarGtk();
  find_bar_controller_.reset(new FindBarController(find_bar_gtk));
  find_bar_gtk->set_find_bar_controller(find_bar_controller_.get());

  contents_container_.reset(
      new TabContentsContainerGtk(find_bar_gtk->widget()));

  contents_container_->AddContainerToBox(vbox_);

  // Note that calling this the first time is necessary to get the
  // proper control layout.
  // TODO(port): make this a pref.
  SetCustomFrame(false);

  status_bubble_.reset(new StatusBubbleGtk(window_));

  gtk_container_add(GTK_CONTAINER(window_), vbox_);
  gtk_widget_show(vbox_);
}

void BrowserWindowGtk::Show() {
  gtk_widget_show(GTK_WIDGET(window_));
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
  // TODO(tc): Once the tab strip model is hooked up, this call can be removed.
  // It should get called by TabDetachedAt when the window is being closed, but
  // we don't have a TabStripModel yet.
  find_bar_controller_->ChangeWebContents(NULL);

  if (!window_)
    return;

  GtkWidget* window = GTK_WIDGET(window_);
  // To help catch bugs in any event handlers that might get fired during the
  // destruction, set window_ to NULL before any handlers will run.
  window_ = NULL;
  gtk_widget_destroy(window);
}

void BrowserWindowGtk::Activate() {
  gtk_window_present(window_);
}

bool BrowserWindowGtk::IsActive() const {
  NOTIMPLEMENTED();
  return true;
}

void BrowserWindowGtk::FlashFrame() {
  // May not be respected by all window managers.
  gtk_window_set_urgency_hint(window_, TRUE);
}

gfx::NativeWindow BrowserWindowGtk::GetNativeHandle() {
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
  if (!window_)
    return;

  std::wstring title = browser_->GetCurrentPageTitle();
  gtk_window_set_title(window_, WideToUTF8(title).c_str());
  if (browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR)) {
    // If we're showing a title bar, we should update the app icon.
    NOTIMPLEMENTED();
  }
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
  // Need to implement full screen mode.
  // http://code.google.com/p/chromium/issues/detail?id=8405
}

bool BrowserWindowGtk::IsFullscreen() const {
  // Need to implement full screen mode.
  // http://code.google.com/p/chromium/issues/detail?id=8405
  return false;
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  return toolbar_->GetLocationBar();
}

void BrowserWindowGtk::SetFocusToLocationBar() {
  GetLocationBar()->FocusLocation();
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

void BrowserWindowGtk::ShowFindBar() {
  find_bar_controller_->Show();
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

    // When dragging the last TabContents out of a window there is no selection
    // notification that causes the find bar for that window to be un-registered
    // for notifications from this TabContents.
    find_bar_controller_->ChangeWebContents(NULL);
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

  if (find_bar_controller_.get())
    find_bar_controller_->ChangeWebContents(new_contents->AsWebContents());
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

void BrowserWindowGtk::ConnectAccelerators() {
  GtkAccelGroup* accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(window_, accel_group);
  // Drop the initial ref on |accel_group| so |window_| will own it.
  g_object_unref(accel_group);

  gtk_accel_group_connect(
      accel_group, kFocusLocationKey, GDK_CONTROL_MASK, GtkAccelFlags(0),
      g_cclosure_new(G_CALLBACK(OnAccelerator), this, NULL));
  gtk_accel_group_connect(
      accel_group, kFocusSearchKey, GDK_CONTROL_MASK, GtkAccelFlags(0),
      g_cclosure_new(G_CALLBACK(OnAccelerator), this, NULL));
  gtk_accel_group_connect(
      accel_group, kOpenFileKey, GDK_CONTROL_MASK, GtkAccelFlags(0),
      g_cclosure_new(G_CALLBACK(OnAccelerator), this, NULL));
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

// static
gboolean BrowserWindowGtk::OnWindowDestroyed(GtkWidget* window,
                                             BrowserWindowGtk* browser_win) {
  // BUG 8712. When we gtk_widget_destroy() in Close(), this will emit the
  // signal right away, and we will be here (while Close() is still in the
  // call stack).  In order to not reenter Close(), and to also follow the
  // expectations of BrowserList, we should run the BrowserWindowGtk destructor
  // not now, but after the run loop goes back to process messages.  Otherwise
  // we will remove ourself from BrowserList while it's being iterated.
  // Additionally, now that we know the window is gone, we need to make sure to
  // set window_ to NULL, otherwise we will try to close the window again when
  // we call Close() in the destructor.
  browser_win->window_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, browser_win);
  return FALSE;  // Don't stop this message.
}

// static
gboolean BrowserWindowGtk::OnAccelerator(GtkAccelGroup* accel_group,
                                         GObject* acceleratable,
                                         guint keyval,
                                         GdkModifierType modifier,
                                         BrowserWindowGtk* browser_window) {
  browser_window->browser_->ExecuteCommand(GetCommandFromKeyval(keyval));
  return TRUE;
}
