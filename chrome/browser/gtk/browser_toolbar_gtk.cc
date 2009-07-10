// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/base_paths_linux.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/gtk/back_forward_button_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/go_button_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Height of the toolbar in pixels.
const int kToolbarHeight = 37;

// Interior spacing between toolbar widgets.
const int kToolbarWidgetSpacing = 4;

// The amount of space between the bottom of the star and the top of the
// Omnibox results popup window.  We want a two pixel space between the bottom
// and the results, but have some extra space below the buttons already.
const int kPopupTopMargin = 0;

// Space between the edge of the star/go button and the popup frame.  We want
// to leave 1 pixel on both side here so that the borders line up.
const int kPopupLeftRightMargin = 1;

}  // namespace

// BrowserToolbarGtk, public ---------------------------------------------------

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser, BrowserWindowGtk* window)
    : toolbar_(NULL),
      location_bar_(new LocationBarViewGtk(browser->command_updater(),
                                           browser->toolbar_model(),
                                           this)),
      model_(browser->toolbar_model()),
      browser_(browser),
      window_(window),
      profile_(NULL),
      last_release_event_flags_(0) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  InitNineBox();
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
  // When we created our MenuGtk objects, we pass them a pointer to our accel
  // group. Make sure to tear them down before |accel_group_|.
  page_menu_.reset();
  app_menu_.reset();
  page_menu_button_.Destroy();
  app_menu_button_.Destroy();
  g_object_unref(accel_group_);
}

void BrowserToolbarGtk::Init(Profile* profile,
                             GtkWindow* top_level_window) {
  // Make sure to tell the location bar the profile before calling its Init.
  SetProfile(profile);

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  event_box_ = gtk_event_box_new();

  toolbar_ = gtk_hbox_new(FALSE, kToolbarWidgetSpacing);
  gtk_container_add(GTK_CONTAINER(event_box_), toolbar_);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_), 4);
  // Demand we're always at least kToolbarHeight tall.
  // -1 for width means "let GTK do its normal sizing".
  gtk_widget_set_size_request(toolbar_, -1, kToolbarHeight);
  g_signal_connect(toolbar_, "expose-event",
                   G_CALLBACK(&OnToolbarExpose), this);

  // A GtkAccelGroup is not InitiallyUnowned, meaning we get a real reference
  // count starting at one.  We don't want the lifetime to be managed by the
  // top level window, since the lifetime should be tied to the C++ object.
  // When we add the accelerator group, the window will take a reference, but
  // we still hold on to the original, and thus own a reference to the group.
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(top_level_window, accel_group_);

  // Group back and forward into an hbox so there's no spacing between them.
  GtkWidget* back_forward_hbox_ = gtk_hbox_new(FALSE, 0);

  back_.reset(new BackForwardButtonGtk(browser_, false));
  gtk_box_pack_start(GTK_BOX(back_forward_hbox_), back_->widget(), FALSE,
                     FALSE, 0);

  forward_.reset(new BackForwardButtonGtk(browser_, true));
  gtk_box_pack_start(GTK_BOX(back_forward_hbox_), forward_->widget(), FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), back_forward_hbox_, FALSE, FALSE, 0);

  reload_.reset(BuildToolbarButton(IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0,
      l10n_util::GetStringUTF8(IDS_TOOLTIP_RELOAD),
      GTK_STOCK_REFRESH));

  home_.reset(BuildToolbarButton(IDR_HOME, IDR_HOME_P, IDR_HOME_H, 0,
                                 l10n_util::GetStringUTF8(IDS_TOOLTIP_HOME),
                                 GTK_STOCK_HOME));
  gtk_util::SetButtonTriggersNavigation(home_->widget());
  SetUpDragForHomeButton();

  // Group the start, omnibox, and go button into an hbox.
  GtkWidget* omnibox_hbox_ = gtk_hbox_new(FALSE, 0);
  star_.reset(BuildStarButton(l10n_util::GetStringUTF8(IDS_TOOLTIP_STAR)));
  gtk_box_pack_start(GTK_BOX(omnibox_hbox_), star_->widget(), FALSE, FALSE, 0);

  location_bar_->Init();
  gtk_box_pack_start(GTK_BOX(omnibox_hbox_), location_bar_->widget(), TRUE,
                     TRUE, 0);

  go_.reset(new GoButtonGtk(location_bar_.get(), browser_));
  gtk_box_pack_start(GTK_BOX(omnibox_hbox_), go_->widget(), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(toolbar_), omnibox_hbox_, TRUE, TRUE, 0);

  // Group the menu buttons together in an hbox.
  GtkWidget* menus_hbox_ = gtk_hbox_new(FALSE, 0);
  GtkWidget* page_menu = BuildToolbarMenuButton(IDR_MENU_PAGE,
      l10n_util::GetStringUTF8(IDS_PAGEMENU_TOOLTIP),
      &page_menu_button_);
  page_menu_.reset(new MenuGtk(this, GetStandardPageMenu(), accel_group_));
  g_signal_connect(page_menu_->widget(), "motion-notify-event",
                   G_CALLBACK(OnPageAppMenuMouseMotion), this);
  g_signal_connect(page_menu_->widget(), "move-current",
                   G_CALLBACK(OnPageAppMenuMoveCurrent), this);
  gtk_box_pack_start(GTK_BOX(menus_hbox_), page_menu, FALSE, FALSE, 0);

  GtkWidget* chrome_menu = BuildToolbarMenuButton(IDR_MENU_CHROME,
      l10n_util::GetStringFUTF8(IDS_APPMENU_TOOLTIP,
          WideToUTF16(l10n_util::GetString(IDS_PRODUCT_NAME))),
      &app_menu_button_);
  app_menu_.reset(new MenuGtk(this, GetStandardAppMenu(), accel_group_));
  g_signal_connect(app_menu_->widget(), "motion-notify-event",
                   G_CALLBACK(OnPageAppMenuMouseMotion), this);
  g_signal_connect(app_menu_->widget(), "move-current",
                   G_CALLBACK(OnPageAppMenuMoveCurrent), this);
  gtk_box_pack_start(GTK_BOX(menus_hbox_), chrome_menu, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(toolbar_), menus_hbox_, FALSE, FALSE, 0);

  // Force all the CustomDrawButtons to load the correct rendering style.
  UserChangedTheme();

  gtk_widget_show_all(event_box_);

  if (show_home_button_.GetValue()) {
    gtk_widget_show(home_->widget());
  } else {
    gtk_widget_hide(home_->widget());
  }
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), event_box_, FALSE, FALSE, 0);
}

void BrowserToolbarGtk::Show() {
  gtk_widget_show(toolbar_);
}

void BrowserToolbarGtk::Hide() {
  gtk_widget_hide(toolbar_);
}

LocationBar* BrowserToolbarGtk::GetLocationBar() const {
  return location_bar_.get();
}

// CommandUpdater::CommandObserver ---------------------------------------------

void BrowserToolbarGtk::EnabledStateChangedForCommand(int id, bool enabled) {
  GtkWidget* widget = NULL;
  switch (id) {
    case IDC_BACK:
      widget = back_->widget();
      break;
    case IDC_FORWARD:
      widget = forward_->widget();
      break;
    case IDC_RELOAD:
      widget = reload_->widget();
      break;
    case IDC_GO:
      widget = go_->widget();
      break;
    case IDC_HOME:
      if (home_.get())
        widget = home_->widget();
      break;
    case IDC_STAR:
      widget = star_->widget();
      break;
  }
  if (widget)
    gtk_widget_set_sensitive(widget, enabled);
}

// MenuGtk::Delegate -----------------------------------------------------------

bool BrowserToolbarGtk::IsCommandEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserToolbarGtk::IsItemChecked(int id) const {
  if (!profile_)
    return false;
  if (id == IDC_SHOW_BOOKMARK_BAR)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  // TODO(port): Fix this when we get some items that want checking!
  return false;
}

void BrowserToolbarGtk::ExecuteCommand(int id) {
  browser_->ExecuteCommand(id);
}

void BrowserToolbarGtk::StoppedShowing() {
  gtk_chrome_button_unset_paint_state(
      GTK_CHROME_BUTTON(page_menu_button_.get()));
  gtk_chrome_button_unset_paint_state(
      GTK_CHROME_BUTTON(app_menu_button_.get()));
}

// NotificationObserver --------------------------------------------------------

void BrowserToolbarGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      if (show_home_button_.GetValue()) {
        gtk_widget_show(home_->widget());
      } else {
        gtk_widget_hide(home_->widget());
      }
    }
  }
}

// BrowserToolbarGtk, public ---------------------------------------------------

void BrowserToolbarGtk::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  location_bar_->SetProfile(profile);
}

void BrowserToolbarGtk::UpdateTabContents(TabContents* contents,
                                          bool should_restore_state) {
  location_bar_->Update(should_restore_state ? contents : NULL);
}

void BrowserToolbarGtk::UserChangedTheme() {
  bool use_gtk = GtkThemeProvider::UseSystemThemeGraphics(profile_);
  back_->SetUseSystemTheme(use_gtk);
  forward_->SetUseSystemTheme(use_gtk);
  reload_->SetUseSystemTheme(use_gtk);
  home_->SetUseSystemTheme(use_gtk);

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(page_menu_button_.get()), use_gtk);
  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(app_menu_button_.get()), use_gtk);
}

gfx::Rect BrowserToolbarGtk::GetPopupBounds() const {
  GtkWidget* star = star_->widget();
  GtkWidget* go = go_->widget();

  // TODO(deanm): The go and star buttons probably share the same window,
  // so this could be optimized to only one origin request.
  gint go_x, go_y;
  gdk_window_get_origin(go->window, &go_x, &go_y);
  go_x += go->allocation.x + go->allocation.width;  // Right edge.

  gint star_x, star_y;
  gdk_window_get_origin(star->window, &star_x, &star_y);
  star_x += star->allocation.x;  // Left edge.
  star_y += star->allocation.y + star->allocation.height;  // Bottom edge.

  return gfx::Rect(star_x + kPopupLeftRightMargin, star_y + kPopupTopMargin,
                   go_x - star_x - (2 * kPopupLeftRightMargin), 0);
}

// BrowserToolbarGtk, private --------------------------------------------------

CustomDrawButton* BrowserToolbarGtk::BuildToolbarButton(
    int normal_id, int active_id, int highlight_id, int depressed_id,
    const std::string& localized_tooltip, const char* stock_id) {
  CustomDrawButton* button = new CustomDrawButton(normal_id, active_id,
      highlight_id, depressed_id, stock_id);

  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  g_signal_connect(button->widget(), "button-release-event",
                   G_CALLBACK(OnButtonRelease), this);

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);
  return button;
}

ToolbarStarToggleGtk* BrowserToolbarGtk::BuildStarButton(
    const std::string& localized_tooltip) {
  ToolbarStarToggleGtk* button = new ToolbarStarToggleGtk(this);

  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  return button;
}

GtkWidget* BrowserToolbarGtk::BuildToolbarMenuButton(
    int icon_id,
    const std::string& localized_tooltip,
    OwnedWidgetGtk* owner) {
  GtkWidget* button = gtk_chrome_button_new();
  owner->Own(button);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (!GtkThemeProvider::UseSystemThemeGraphics(profile_))
    gtk_container_set_border_width(GTK_CONTAINER(button), 2);
  gtk_container_add(GTK_CONTAINER(button),
                    gtk_image_new_from_pixbuf(rb.GetPixbufNamed(icon_id)));

  gtk_widget_set_tooltip_text(button, localized_tooltip.c_str());
  g_signal_connect(button, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);

  return button;
}

void BrowserToolbarGtk::SetUpDragForHomeButton() {
  // TODO(estade): we should use a custom drag-drop handler so that we can
  // prefer URIs over plain text when both are available.
  gtk_drag_dest_set(home_->widget(), GTK_DEST_DEFAULT_ALL,
                    NULL, 0, GDK_ACTION_COPY);
  GtkDndUtil::SetDestTargetListFromCodeMask(home_->widget(),
                                            GtkDndUtil::X_CHROME_TEXT_PLAIN |
                                            GtkDndUtil::X_CHROME_TEXT_URI_LIST);

  g_signal_connect(home_->widget(), "drag-data-received",
                   G_CALLBACK(OnDragDataReceived), this);
}

void BrowserToolbarGtk::ChangeActiveMenu(GtkWidget* active_menu,
    guint timestamp) {
  MenuGtk* old_menu;
  MenuGtk* new_menu;
  GtkWidget* relevant_button;
  if (active_menu == app_menu_->widget()) {
    old_menu = app_menu_.get();
    new_menu = page_menu_.get();
    relevant_button = page_menu_button_.get();
  } else {
    old_menu = page_menu_.get();
    new_menu = app_menu_.get();
    relevant_button = app_menu_button_.get();
  }

  old_menu->Cancel();
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(relevant_button),
                                    GTK_STATE_ACTIVE);
  new_menu->Popup(relevant_button, 0, timestamp);
}

// static
gboolean BrowserToolbarGtk::OnToolbarExpose(GtkWidget* widget,
                                            GdkEventExpose* e,
                                            BrowserToolbarGtk* toolbar) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, e->area.x, e->area.y, e->area.width, e->area.height);
  cairo_clip(cr);
  // The toolbar is supposed to blend in with the active tab, so we have to pass
  // coordinates for the IDR_THEME_TOOLBAR bitmap relative to the top of the
  // tab strip.
  gfx::Point tabstrip_origin =
      toolbar->window_->tabstrip()->GetTabStripOriginForWidget(widget);
  toolbar->background_ninebox_->RenderTopCenterStrip(
      cr, tabstrip_origin.x(), tabstrip_origin.y(),
      e->area.x + e->area.width - tabstrip_origin.x());
  cairo_destroy(cr);

  return FALSE;  // Allow subwidgets to paint.
}

// static
void BrowserToolbarGtk::OnButtonClick(GtkWidget* button,
                                      BrowserToolbarGtk* toolbar) {
  int tag = -1;
  if (button == toolbar->reload_->widget())
    tag = IDC_RELOAD;
  else if (toolbar->home_.get() && button == toolbar->home_->widget())
    tag = IDC_HOME;
  else if (button == toolbar->star_->widget())
    tag = IDC_STAR;

  DCHECK_NE(tag, -1) << "Unexpected button click callback";
  toolbar->browser_->ExecuteCommandWithDisposition(tag,
      event_utils::DispositionFromEventFlags(
          toolbar->last_release_event_flags_));
}

// static
gboolean BrowserToolbarGtk::OnButtonRelease(GtkWidget* button,
                                            GdkEventButton* event,
                                            BrowserToolbarGtk* toolbar) {
  toolbar->last_release_event_flags_ = event->state;
  return FALSE;
}

// static
gboolean BrowserToolbarGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                   GdkEventButton* event,
                                                   BrowserToolbarGtk* toolbar) {
  if (event->button != 1)
    return FALSE;

  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(button),
                                    GTK_STATE_ACTIVE);
  MenuGtk* menu = button == toolbar->page_menu_button_.get() ?
                  toolbar->page_menu_.get() : toolbar->app_menu_.get();
  menu->Popup(button, reinterpret_cast<GdkEvent*>(event));

  return TRUE;
}

// static
void BrowserToolbarGtk::OnDragDataReceived(GtkWidget* widget,
    GdkDragContext* drag_context, gint x, gint y,
    GtkSelectionData* data, guint info, guint time,
    BrowserToolbarGtk* toolbar) {
  if (info != GtkDndUtil::X_CHROME_TEXT_PLAIN) {
    NOTIMPLEMENTED() << "Only support plain text drops for now, sorry!";
    return;
  }

  GURL url(reinterpret_cast<char*>(data->data));
  if (!url.is_valid())
    return;

  bool url_is_newtab = url.spec() == chrome::kChromeUINewTabURL;
  toolbar->profile_->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                            url_is_newtab);
  if (!url_is_newtab) {
    toolbar->profile_->GetPrefs()->SetString(prefs::kHomePage,
                                             UTF8ToWide(url.spec()));
  }
}

// static
gboolean BrowserToolbarGtk::OnPageAppMenuMouseMotion(GtkWidget* menu,
    GdkEventMotion* event, BrowserToolbarGtk* toolbar) {
  if (gtk_util::WidgetContainsCursor(menu == toolbar->app_menu_->widget() ?
                                     toolbar->page_menu_button_.get() :
                                     toolbar->app_menu_button_.get())) {
    toolbar->ChangeActiveMenu(menu, event->time);
    return TRUE;
  }

  return FALSE;
}

// static
void BrowserToolbarGtk::OnPageAppMenuMoveCurrent(GtkWidget* menu,
    GtkMenuDirectionType dir, BrowserToolbarGtk* toolbar) {
  GtkWidget* active_item = GTK_MENU_SHELL(menu)->active_menu_item;

  switch (dir) {
    case GTK_MENU_DIR_CHILD:
      // The move is going to open a submenu; don't override default behavior.
      if (active_item && gtk_menu_item_get_submenu(GTK_MENU_ITEM(active_item)))
        break;
      // Fall through.
    case GTK_MENU_DIR_PARENT:
      toolbar->ChangeActiveMenu(menu, gtk_get_current_event_time());
      // This signal doesn't have a return value; we have to manually stop its
      // propagation.
      g_signal_stop_emission_by_name(menu, "move-current");
    default:
      break;
  }
}

void BrowserToolbarGtk::InitNineBox() {
  // TODO(estade): use |profile_|?
  background_ninebox_.reset(new NineBox(
      browser_->profile()->GetThemeProvider(),
      0, IDR_THEME_TOOLBAR, 0, 0, 0, 0, 0, 0, 0));
}
