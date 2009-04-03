// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/base_paths_linux.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/find_bar_gtk.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/controls/button/text_button.h"
#include "grit/theme_resources.h"

namespace {

class DummyButtonListener : public views::ButtonListener {
 public:
  virtual void ButtonPressed(views::Button* sender) {
    DLOG(ERROR) << "Button Pressed!";
  }
};

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

// Callback for the delete event.  This event is fired when the user tries to
// close the window (e.g., clicking on the X in the window manager title bar).
gboolean MainWindowDeleteEvent(GtkWidget* widget, GdkEvent* event,
                               BrowserWindowGtk* window) {
  window->Close();

  // Return true to prevent the gtk window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

void MainWindowDestroy(GtkWidget* widget, BrowserWindowGtk* window) {
  // BUG 8712. When we gtk_widget_destroy() in Close(), this will emit the
  // signal right away, and we will be here (while Close() is still in the
  // call stack).  In order to not reenter Close(), and to also follow the
  // expectations of BrowserList, we should run the BrowserWindowGtk destructor
  // not now, but after the run loop goes back to process messages.  Otherwise
  // we will remove ourself from BrowserList while it's being iterated.
  // Additionally, now that we know the window is gone, we need to make sure to
  // set window_ to NULL, otherwise we will try to close the window again when
  // we call Close() in the destructor.
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
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

const struct AcceleratorMapping {
  guint keyval;
  int command_id;
} kAcceleratorMap[] = {
  { GDK_k, IDC_FOCUS_SEARCH },
  { GDK_l, IDC_FOCUS_LOCATION },
  { GDK_o, IDC_OPEN_FILE },
  { GDK_Page_Down, IDC_SELECT_NEXT_TAB },
  { GDK_Page_Up, IDC_SELECT_PREVIOUS_TAB },
  { GDK_w, IDC_CLOSE_TAB },
};

int GetCommandFromKeyval(guint accel_key) {
  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    if (kAcceleratorMap[i].keyval == accel_key)
      return kAcceleratorMap[i].command_id;
  }
  NOTREACHED();
  return 0;
}

// Usually accelerators are checked before propagating the key event, but if the
// focus is on the render area we want to reverse the order of things to allow
// webkit to handle key events like ctrl-l.
gboolean OnKeyPress(GtkWindow* window, GdkEventKey* event, Browser* browser) {
  TabContents* current_tab_contents =
      browser->tabstrip_model()->GetSelectedTabContents();
  // If there is no current tab contents or it is not focused then let the
  // default GtkWindow key handler run.
  if (!current_tab_contents)
    return FALSE;
  if (!gtk_widget_is_focus(current_tab_contents->GetContentNativeView()))
    return FALSE;

  // If the content area is focused, let it handle the key event.
  gboolean result = gtk_window_propagate_key_event(window, event);
  DCHECK(result);
  return TRUE;
}

}  // namespace

// TODO(estade): Break up this constructor into helper functions to improve
// readability.
BrowserWindowGtk::BrowserWindowGtk(Browser* browser)
    :  browser_(browser),
       // TODO(port): make this a pref.
       custom_frame_(false),
       method_factory_(this) {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_default_size(window_, 640, 480);
  g_object_set_data(G_OBJECT(window_), "browser_window_gtk", this);
  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(MainWindowDeleteEvent), this);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(MainWindowDestroy), this);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(MainWindowConfigured), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(MainWindowStateChanged), this);
  g_signal_connect(window_, "key-press-event",
                   G_CALLBACK(OnKeyPress), browser_.get());
  ConnectAccelerators();
  bounds_ = GetInitialWindowBounds(window_);

  // This vbox encompasses all of the widgets within the browser, including
  // the tabstrip and the content vbox.
  window_vbox_ = gtk_vbox_new(FALSE, 0);

  tabstrip_.reset(new TabStripGtk(browser_->tabstrip_model()));
  tabstrip_->Init();
  tabstrip_->AddTabStripToBox(window_vbox_);

  // This vbox surrounds the "content": toolbar+page.
  content_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_app_paintable(content_vbox_, TRUE);
  gtk_widget_set_double_buffered(content_vbox_, FALSE);
  g_signal_connect(G_OBJECT(content_vbox_), "expose-event",
                   G_CALLBACK(&OnContentAreaExpose), this);

  // Temporary hack hidden behind a command line option to add one of the
  // experimental ViewsGtk objects to the Gtk hierarchy.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kViewsGtk)) {
    experimental_widget_.reset(new views::WidgetGtk());
    experimental_widget_->Init(gfx::Rect(), false);
    experimental_widget_->SetContentsView(
        new views::TextButton(new DummyButtonListener, L"Button"));

    gtk_box_pack_start(GTK_BOX(content_vbox_),
                       experimental_widget_->GetNativeView(),
                       false, false, 2);
  }

  toolbar_.reset(new BrowserToolbarGtk(browser_.get()));
  toolbar_->Init(browser_->profile(), window_);
  toolbar_->AddToolbarToBox(content_vbox_);

  FindBarGtk* find_bar_gtk = new FindBarGtk();
  find_bar_controller_.reset(new FindBarController(find_bar_gtk));
  find_bar_gtk->set_find_bar_controller(find_bar_controller_.get());

  contents_container_.reset(
      new TabContentsContainerGtk(find_bar_gtk->widget()));

  contents_container_->AddContainerToBox(content_vbox_);

  // Note that calling this the first time is necessary to get the
  // proper control layout.
  // TODO(port): make this a pref.
  SetCustomFrame(false);

  status_bubble_.reset(new StatusBubbleGtk(window_));

  gtk_container_add(GTK_CONTAINER(window_vbox_), content_vbox_);
  gtk_container_add(GTK_CONTAINER(window_), window_vbox_);
  gtk_widget_show(content_vbox_);
  gtk_widget_show(window_vbox_);
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserWindowGtk::~BrowserWindowGtk() {
  browser_->tabstrip_model()->RemoveObserver(this);
}

void BrowserWindowGtk::HandleAccelerator(guint keyval,
                                         GdkModifierType modifier) {
  // Filter modifier to only include accelerator modifiers.
  modifier = static_cast<GdkModifierType>(
      modifier & gtk_accelerator_get_default_mod_mask());
  switch (keyval) {
    // Gtk doesn't allow GDK_Tab or GDK_ISO_Left_Tab to be an accelerator (see
    // gtk_accelerator_valid), so we need to handle these accelerators
    // manually.
    case GDK_Tab:
      if (GDK_CONTROL_MASK == modifier)
        ExecuteBrowserCommand(IDC_SELECT_NEXT_TAB);
      break;

    case GDK_ISO_Left_Tab:
      if ((GDK_CONTROL_MASK | GDK_SHIFT_MASK) == modifier)
        ExecuteBrowserCommand(IDC_SELECT_PREVIOUS_TAB);
      break;

    default:
      // Pass the accelerator on to GTK.
      gtk_accel_groups_activate(G_OBJECT(window_), keyval, modifier);
  }
}

gboolean BrowserWindowGtk::OnContentAreaExpose(GtkWidget* widget,
                                               GdkEventExpose* e,
                                               BrowserWindowGtk* window) {
  if (window->custom_frame_) {
    NOTIMPLEMENTED() << " needs custom drawing for the custom frame.";
    return FALSE;
  }

  return FALSE;  // Allow subwidgets to paint.
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
  if (!CanClose())
    return;

  // TODO(tc): We should store the window position, perhaps using
  // gtk_window_set_role.
  //SaveWindowPosition();

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
  std::wstring title = browser_->GetCurrentPageTitle();
  gtk_window_set_title(window_, WideToUTF8(title).c_str());
  if (browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR)) {
    // If we're showing a title bar, we should update the app icon.
    NOTIMPLEMENTED();
  }
}

void BrowserWindowGtk::UpdateLoadingAnimations(bool should_animate) {
  // Need to implement loading animations.
  // http://code.google.com/p/chromium/issues/detail?id=9380
}

void BrowserWindowGtk::SetStarredState(bool is_starred) {
  // Hide our current lack of starring support from NOTIMPLEMENTED spew.
  if (is_starred)
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
  // Need to implement stop/go button.
  // http://code.google.com/p/chromium/issues/detail?id=9381
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

void BrowserWindowGtk::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
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

// TODO(estade): this function should probably be unforked from the BrowserView
// function of the same name by having a shared partial BrowserWindow
// implementation.
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
  // TODO(estade): after we manage browser activation, add a check to make sure
  // we are the active browser before calling RestoreFocus().
  if (!browser_->tabstrip_model()->closing_all() &&
      new_contents->AsWebContents()) {
    new_contents->AsWebContents()->view()->RestoreFocus();
  }

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

bool BrowserWindowGtk::CanClose() const {
  // TODO(tc): We don't have tab dragging yet.
  // You cannot close a frame for which there is an active originating drag
  // session.
  //if (tabstrip_->IsDragSessionActive())
  //  return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the window (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    gtk_widget_hide(GTK_WIDGET(window_));
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  NotificationService::current()->Notify(
      NotificationType::WINDOW_CLOSED,
      Source<GtkWindow>(window_),
      NotificationService::NoDetails());
  return true;
}

void BrowserWindowGtk::ConnectAccelerators() {
  GtkAccelGroup* accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(window_, accel_group);
  // Drop the initial ref on |accel_group| so |window_| will own it.
  g_object_unref(accel_group);

  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    gtk_accel_group_connect(
        accel_group, kAcceleratorMap[i].keyval, GDK_CONTROL_MASK,
        GtkAccelFlags(0), g_cclosure_new(G_CALLBACK(OnGtkAccelerator),
        this, NULL));
  }
}

void BrowserWindowGtk::SetCustomFrame(bool custom_frame) {
  custom_frame_ = custom_frame;
  if (custom_frame_) {
    gtk_container_set_border_width(GTK_CONTAINER(window_vbox_), 2);
    // TODO(port): all the crazy blue title bar, etc.
    NOTIMPLEMENTED();
  } else {
    gtk_container_set_border_width(GTK_CONTAINER(window_vbox_), 0);
  }
}

// static
gboolean BrowserWindowGtk::OnGtkAccelerator(GtkAccelGroup* accel_group,
                                            GObject* acceleratable,
                                            guint keyval,
                                            GdkModifierType modifier,
                                            BrowserWindowGtk* browser_window) {
  int command_id = GetCommandFromKeyval(keyval);
  // We have to delay certain commands that may try to destroy widgets to which
  // GTK is currently holding a reference. (For now the only such command is
  // tab closing.) GTK will hold a reference on the RWHV widget when the
  // event came through on that widget but GTK focus was elsewhere.
  if (IDC_CLOSE_TAB == command_id) {
    MessageLoop::current()->PostTask(FROM_HERE,
        browser_window->method_factory_.NewRunnableMethod(
            &BrowserWindowGtk::ExecuteBrowserCommand,
            command_id));
  } else {
    browser_window->ExecuteBrowserCommand(command_id);
  }

  return TRUE;
}

void BrowserWindowGtk::ExecuteBrowserCommand(int id) {
  browser_->ExecuteCommand(id);
}
