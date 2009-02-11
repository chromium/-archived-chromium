// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_view_gtk.h"

#include "base/logging.h"
#include "base/base_paths_linux.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/common/l10n_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// CustomDrawButton manages the lifetimes of some resources used to make a
// custom-drawn Gtk button.  We use them on the toolbar.
class BrowserToolbarGtk::CustomDrawButton {
 public:
  // The constructor takes a filename, which is used as the base filename
  // in loading the theme graphics pngs.  This will be replaced by the
  // ResourceBundle graphics soon.
  CustomDrawButton(const std::string& filename);
  ~CustomDrawButton();

  GtkWidget* widget() const { return widget_; }

 private:
  // Load an image from a path.
  // TODO(port): Temporary until ResourceBundle works.
  GdkPixbuf* LoadImage(const std::string& filename);

  // Load all the button images from a base theme filename.
  // TODO(port): Temporary until ResourceBundle works.
  void LoadImages(const std::string& filename);

  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           CustomDrawButton* obj);

  // The actual button widget.
  GtkWidget* widget_;

  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];
};

BrowserToolbarGtk::CustomDrawButton::CustomDrawButton(
    const std::string& filename) {
  widget_ = gtk_button_new();
  LoadImages(filename);

  gtk_widget_set_app_paintable(widget_, TRUE);
  g_signal_connect(G_OBJECT(widget_), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

BrowserToolbarGtk::CustomDrawButton::~CustomDrawButton() {
  for (size_t i = 0; i < arraysize(pixbufs_); ++i) {
    if (pixbufs_[i])
      gdk_pixbuf_unref(pixbufs_[i]);
  }
}

GdkPixbuf* BrowserToolbarGtk::CustomDrawButton::LoadImage(
    const std::string& filename) {
  // We intentionally ignore errors here, as some buttons don't have images
  // for all states.  This will all be removed once ResourceBundle works.
  return gdk_pixbuf_new_from_file(filename.c_str(), NULL);
}

void BrowserToolbarGtk::CustomDrawButton::LoadImages(
    const std::string& filename) {
  // TODO(evanm): make this use ResourceBundle once that is ported.
  FilePath path;
  bool ok = PathService::Get(base::DIR_SOURCE_ROOT, &path);
  DCHECK(ok);
  path = path.Append("chrome/app/theme").Append(filename);
  pixbufs_[GTK_STATE_NORMAL] = LoadImage(path.value() + ".png");
  pixbufs_[GTK_STATE_ACTIVE] = LoadImage(path.value() + "_p.png");
  pixbufs_[GTK_STATE_PRELIGHT] = LoadImage(path.value() + "_h.png");
  pixbufs_[GTK_STATE_SELECTED] = NULL;
  pixbufs_[GTK_STATE_INSENSITIVE] = LoadImage(path.value() + "_d.png");

  gtk_widget_set_size_request(widget_,
                              gdk_pixbuf_get_width(pixbufs_[0]),
                              gdk_pixbuf_get_height(pixbufs_[0]));
}

// static
gboolean BrowserToolbarGtk::CustomDrawButton::OnExpose(
    GtkWidget* widget,
    GdkEventExpose* e,
    CustomDrawButton* button) {
  GdkPixbuf* pixbuf = button->pixbufs_[GTK_WIDGET_STATE(widget)];

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf)
    pixbuf = button->pixbufs_[GTK_STATE_NORMAL];

  if (!pixbuf)
    return FALSE;

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);
  return TRUE;
}

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser)
    : toolbar_(NULL),
      model_(browser->toolbar_model()),
      browser_(browser),
      profile_(NULL) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  // TODO(port): Port BackForwardMenuModel
  // back_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  // forward_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
}

void BrowserToolbarGtk::Init(Profile* profile) {
  toolbar_ = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_), 6);

  toolbar_tooltips_ = gtk_tooltips_new();

  back_.reset(BuildToolbarButton("back",
                                 l10n_util::GetString(IDS_TOOLTIP_BACK)));
  forward_.reset(BuildToolbarButton("forward",
                                 l10n_util::GetString(IDS_TOOLTIP_FORWARD)));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new(" "), FALSE, FALSE, 0);

  reload_.reset(BuildToolbarButton("reload",
                                   l10n_util::GetString(IDS_TOOLTIP_RELOAD)));
  home_.reset(BuildToolbarButton("home",
                                 l10n_util::GetString(IDS_TOOLTIP_HOME)));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new("  "), FALSE, FALSE, 0);

  star_.reset(BuildToolbarButton("star",
                                 l10n_util::GetString(IDS_TOOLTIP_STAR)));

  GtkWidget* entry = gtk_entry_new();
  gtk_widget_set_size_request(entry, 0, 27);
  gtk_box_pack_start(GTK_BOX(toolbar_), entry, TRUE, TRUE, 0);

  go_.reset(BuildToolbarButton("go", L""));

  // TODO(port): these buttons need even stranger drawing than the others.
  page_menu_.reset(BuildToolbarButton("menu_page",
      l10n_util::GetString(IDS_PAGEMENU_TOOLTIP)));

  // TODO(port): Need to get l10n_util::GetStringF working under linux to get
  // the correct string here.
  app_menu_.reset(BuildToolbarButton("menu_chrome",
      l10n_util::GetString(IDS_APPMENU_TOOLTIP)));

  // TODO(erg): wchar_t mismatch on linux. Fix later.
  //  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), toolbar_, FALSE, FALSE, 0);
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
    case IDC_HOME:
      widget = home_->widget();
      break;
    case IDC_STAR:
      widget = star_->widget();
      break;
  }
  if (widget)
    gtk_widget_set_sensitive(widget, enabled);
}

void BrowserToolbarGtk::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  // TODO(erg): location_bar_ is a normal gtk text box right now. Change this
  // when we get omnibox support.
  //  location_bar_->SetProfile(profile);
}

// TODO(port): This needs to deal with our styled pixmaps.
BrowserToolbarGtk::CustomDrawButton* BrowserToolbarGtk::BuildToolbarButton(
    const std::string& filename,
    const std::wstring& localized_tooltip) {
  CustomDrawButton* button = new CustomDrawButton(filename);

  // TODO(erg): Mismatch between wstring and string.
  // gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tooltips_),
  //                      GTK_WIDGET(back_),
  //                      localized_tooltip, localized_tooltip);
  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(ButtonClickCallback), this);
  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);

  return button;
}

/* static */
void BrowserToolbarGtk::ButtonClickCallback(GtkWidget* button,
                                            BrowserToolbarGtk* toolbar) {
  int tag = -1;
  if (button == toolbar->back_->widget())
    tag = IDC_BACK;
  else if (button == toolbar->forward_->widget())
    tag = IDC_FORWARD;
  else if (button == toolbar->reload_->widget())
    tag = IDC_RELOAD;
  else if (button == toolbar->home_->widget())
    tag = IDC_HOME;
  else if (button == toolbar->star_->widget())
    tag = IDC_STAR;

  if (tag != -1) {
    toolbar->browser_->ExecuteCommand(tag);
  } else {
    // TODO(erg): The menu buttons are special; they need to spawn menus.
  }
}
