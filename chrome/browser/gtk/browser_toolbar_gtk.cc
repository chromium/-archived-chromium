// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/logging.h"
#include "base/base_paths_linux.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/back_forward_menu_model_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

const int BrowserToolbarGtk::kToolbarHeight = 38;
// For the back/forward dropdown menus, the time in milliseconds between
// when the user clicks and the popup menu appears.
static const int kMenuTimerDelay = 500;

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser)
    : toolbar_(NULL),
      location_bar_(new LocationBarViewGtk(browser->command_updater(),
                                           browser->toolbar_model())),
      model_(browser->toolbar_model()),
      browser_(browser),
      profile_(NULL),
      show_menu_factory_(this) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  back_menu_model_.reset(new BackForwardMenuModelGtk(
      browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  forward_menu_model_.reset(new BackForwardMenuModelGtk(
      browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
  // When we created our MenuGtk objects, we pass them a pointer to our accel
  // group.  Make sure to tear them down before |accel_group_|.
  page_menu_.reset();
  app_menu_.reset();
  back_forward_menu_.reset();
  g_object_unref(accel_group_);
}

void BrowserToolbarGtk::Init(Profile* profile,
                             GtkWindow* top_level_window) {
  // Make sure to tell the location bar the profile before calling its Init.
  SetProfile(profile);

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  toolbar_ = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_), 4);
  // Demand we're always at least kToolbarHeight tall.
  // -1 for width means "let GTK do its normal sizing".
  gtk_widget_set_size_request(toolbar_, -1, kToolbarHeight);

  // A GtkAccelGroup is not InitiallyUnowned, meaning we get a real reference
  // count starting at one.  We don't want the lifetime to be managed by the
  // top level window, since the lifetime should be tied to the C++ object.
  // When we add the accelerator group, the window will take a reference, but
  // we still hold on to the original, and thus own a reference to the group.
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(top_level_window, accel_group_);

  back_.reset(BuildBackForwardButton(IDR_BACK, IDR_BACK_P, IDR_BACK_H,
              IDR_BACK_D,
              l10n_util::GetString(IDS_TOOLTIP_BACK)));
  AddAcceleratorToButton(back_, GDK_Left, GDK_MOD1_MASK);
  forward_.reset(BuildBackForwardButton(IDR_FORWARD, IDR_FORWARD_P,
                 IDR_FORWARD_H, IDR_FORWARD_D,
                 l10n_util::GetString(IDS_TOOLTIP_FORWARD)));
  AddAcceleratorToButton(forward_, GDK_Right, GDK_MOD1_MASK);

  // TODO(estade): These blank labels are kind of ghetto. Padding should be
  // handled differently (via spacing parameters or padding widgets that use
  // gtk_widget_set_size_request).
  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new(" "), FALSE, FALSE, 0);

  reload_.reset(BuildToolbarButton(IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0,
      l10n_util::GetString(IDS_TOOLTIP_RELOAD)));
  AddAcceleratorToButton(reload_, GDK_r, GDK_CONTROL_MASK);

  // TODO(port): we need to dynamically react to changes in show_home_button_
  // and hide/show home appropriately.  But we don't have a UI for it yet.
  if (*show_home_button_)
    home_.reset(MakeHomeButton());

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new("  "), FALSE, FALSE, 0);

  star_.reset(BuildToolbarButton(IDR_STAR, IDR_STAR_P, IDR_STAR_H, IDR_STAR_D,
      l10n_util::GetString(IDS_TOOLTIP_STAR)));

  location_bar_->Init();
  gtk_box_pack_start(GTK_BOX(toolbar_), location_bar_->widget(), TRUE, TRUE, 0);

  go_.reset(BuildToolbarButton(IDR_GO, IDR_GO_P, IDR_GO_H, 0, L""));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new(" "), FALSE, FALSE, 0);

  page_menu_button_.reset(BuildToolbarMenuButton(IDR_MENU_PAGE,
      l10n_util::GetString(IDS_PAGEMENU_TOOLTIP)));
  page_menu_.reset(new MenuGtk(this, GetStandardPageMenu(), accel_group_));

  app_menu_button_.reset(BuildToolbarMenuButton(IDR_MENU_CHROME,
      l10n_util::GetStringF(IDS_APPMENU_TOOLTIP,
                            l10n_util::GetString(IDS_PRODUCT_NAME))));
  app_menu_.reset(new MenuGtk(this, GetStandardAppMenu(), accel_group_));

  gtk_widget_show_all(toolbar_);
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), toolbar_, FALSE, FALSE, 0);
}

LocationBar* BrowserToolbarGtk::GetLocationBar() const {
  return location_bar_.get();
}

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

void BrowserToolbarGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      // TODO(port): add/remove home button.
      NOTIMPLEMENTED();
    }
  }
}

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

CustomDrawButton* BrowserToolbarGtk::BuildToolbarButton(
    int normal_id, int active_id, int highlight_id, int depressed_id,
    const std::wstring& localized_tooltip) {
  CustomDrawButton* button = new CustomDrawButton(normal_id, active_id,
      highlight_id, depressed_id);

  gtk_widget_set_tooltip_text(button->widget(),
                              WideToUTF8(localized_tooltip).c_str());
  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);
  return button;
}

CustomContainerButton* BrowserToolbarGtk::BuildToolbarMenuButton(
    int icon_id,
    const std::wstring& localized_tooltip) {
  CustomContainerButton* button = new CustomContainerButton;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gtk_container_set_border_width(GTK_CONTAINER(button->widget()), 2);
  gtk_container_add(GTK_CONTAINER(button->widget()),
                    gtk_image_new_from_pixbuf(rb.LoadPixbuf(icon_id)));

  gtk_widget_set_tooltip_text(button->widget(),
                              WideToUTF8(localized_tooltip).c_str());
  g_signal_connect(G_OBJECT(button->widget()), "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);

  return button;
}

// static
void BrowserToolbarGtk::OnButtonClick(GtkWidget* button,
                                      BrowserToolbarGtk* toolbar) {
  int tag = -1;
  if (button == toolbar->back_->widget())
    tag = IDC_BACK;
  else if (button == toolbar->forward_->widget())
    tag = IDC_FORWARD;
  else if (button == toolbar->reload_->widget())
    tag = IDC_RELOAD;
  else if (button == toolbar->go_->widget())
    tag = IDC_GO;
  else if (toolbar->home_.get() && button == toolbar->home_->widget())
    tag = IDC_HOME;
  else if (button == toolbar->star_->widget())
    tag = IDC_STAR;

  if (tag == IDC_BACK || tag == IDC_FORWARD)
    toolbar->show_menu_factory_.RevokeAll();

  DCHECK(tag != -1) << "Impossible button click callback";
  toolbar->browser_->ExecuteCommand(tag);
}

// static
gboolean BrowserToolbarGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                   GdkEvent* event,
                                                   BrowserToolbarGtk* toolbar) {
  // TODO(port): this never puts the button into the "active" state,
  // which means we never display the button-pressed-down graphics.  I
  // suspect a better way to do it is just to use a real GtkMenuShell
  // with our custom drawing.
  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      // We have a button press we should respond to.
      if (button == toolbar->page_menu_button_->widget()) {
        toolbar->RunPageMenu(event);
        return TRUE;
      } else if (button == toolbar->app_menu_button_->widget()) {
        toolbar->RunAppMenu(event);
        return TRUE;
      }
    }
  }

  return FALSE;
}

CustomDrawButton* BrowserToolbarGtk::BuildBackForwardButton(
    int normal_id,
    int active_id,
    int highlight_id,
    int depressed_id,
    const std::wstring& localized_tooltip) {
  CustomDrawButton* button = new CustomDrawButton(normal_id, active_id,
                                                  highlight_id, depressed_id);

  gtk_widget_set_tooltip_text(button->widget(),
                              WideToUTF8(localized_tooltip).c_str());

  g_signal_connect(G_OBJECT(button->widget()), "button-press-event",
                   G_CALLBACK(OnBackForwardPressEvent), this);
  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);
  // Popup the menu as left-aligned relative to this widget rather than the
  // default of right aligned.
  g_object_set_data(G_OBJECT(button->widget()), "left-align-popup",
                    reinterpret_cast<void*>(true));
  return button;
}

void BrowserToolbarGtk::AddAcceleratorToButton(
    const scoped_ptr<CustomDrawButton>& button,
    unsigned int accelerator,
    unsigned int accelerator_mod) {
  gtk_widget_add_accelerator(
      button->widget(), "clicked", accel_group_, accelerator,
      GdkModifierType(accelerator_mod), GtkAccelFlags(0));
}

// static
gboolean BrowserToolbarGtk::OnBackForwardPressEvent(GtkWidget* widget,
    GdkEventButton* event,
    BrowserToolbarGtk* toolbar) {
  // TODO(port): only allow left clicks to open the menu.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      toolbar->show_menu_factory_.NewRunnableMethod(
          &BrowserToolbarGtk::ShowBackForwardMenu,
          widget, event->button),
      kMenuTimerDelay);
  return FALSE;
}

void BrowserToolbarGtk::ShowBackForwardMenu(GtkWidget* widget,
                                            gint button_type) {
  if (widget == back_->widget()) {
    back_forward_menu_.reset(new MenuGtk(back_menu_model_.get()));
  } else {
    back_forward_menu_.reset(new MenuGtk(forward_menu_model_.get()));
  }

  back_forward_menu_->Popup(widget, button_type, gtk_get_current_event_time());
}

void BrowserToolbarGtk::RunPageMenu(GdkEvent* button_press_event) {
  page_menu_->Popup(page_menu_button_->widget(), button_press_event);
}

void BrowserToolbarGtk::RunAppMenu(GdkEvent* button_press_event) {
  app_menu_->Popup(app_menu_button_->widget(), button_press_event);
}

CustomDrawButton* BrowserToolbarGtk::MakeHomeButton() {
  return BuildToolbarButton(IDR_HOME, IDR_HOME_P, IDR_HOME_H, 0,
                            l10n_util::GetString(IDS_TOOLTIP_HOME));
}
