// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_titlebar.h"

#include <gtk/gtk.h>

#include "app/resource_bundle.h"
#include "app/l10n_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The space above the titlebars.
const int kTitlebarHeight = 14;

// A linux specific menu item for toggling window decorations.
const int kShowWindowDecorationsCommand = 200;

// The following OTR constants copied from opaque_browser_frame_view.cc:
// In maximized mode, the OTR avatar starts 2 px below the top of the screen, so
// that it doesn't extend into the "3D edge" portion of the titlebar.
const int kOTRMaximizedTopSpacing = 2;
// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;
// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;

gboolean OnMouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                          BrowserWindowGtk* browser_window) {
  // Reset to the default mouse cursor.
  browser_window->ResetCustomFrameCursor();
  return TRUE;
}

GdkPixbuf* GetOTRAvatar() {
  static GdkPixbuf* otr_avatar = NULL;
  if (!otr_avatar) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    otr_avatar = rb.GetRTLEnabledPixbufNamed(IDR_OTR_ICON);
  }
  return otr_avatar;
}

}  // namespace

BrowserTitlebar::BrowserTitlebar(BrowserWindowGtk* browser_window,
                                 GtkWindow* window)
    : browser_window_(browser_window), window_(window),
      using_custom_frame_(false) {
  Init();
}

void BrowserTitlebar::Init() {
  // The widget hierarchy is shown below.
  //
  // +- HBox (container_) -----------------------------------------------------+
  // |+- Algn. -++- Alignment --------------++- VBox (titlebar_buttons_box_) -+|
  // ||+ Image +||   (titlebar_alignment_)  ||+- HBox -----------------------+||
  // |||spy_guy|||                          |||+- button -++- button -+      |||
  // |||       |||+- TabStripGtk  ---------+|||| minimize || restore  | ...  |||
  // |||  )8\  |||| tab   tab   tabclose   ||||+----------++----------+      |||
  // ||+-------+||+------------------------+||+------------------------------+||
  // |+---------++--------------------------++--------------------------------+|
  // +-------------------------------------------------------------------------+
  container_ = gtk_hbox_new(FALSE, 0);

  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateChanged), this);

  if (browser_window_->browser()->profile()->IsOffTheRecord()) {
    GtkWidget* spy_guy = gtk_image_new_from_pixbuf(GetOTRAvatar());
    gtk_misc_set_alignment(GTK_MISC(spy_guy), 0.0, 1.0);
    GtkWidget* spy_frame = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    // We use this alignment rather than setting padding on the GtkImage because
    // the image's intrinsic padding doesn't clip the pixbuf during painting.
    gtk_alignment_set_padding(GTK_ALIGNMENT(spy_frame), kOTRMaximizedTopSpacing,
        kOTRBottomSpacing, kOTRSideSpacing, kOTRSideSpacing);
    gtk_widget_set_size_request(spy_guy, -1, 0);
    gtk_container_add(GTK_CONTAINER(spy_frame), spy_guy);
    gtk_box_pack_start(GTK_BOX(container_), spy_frame, FALSE, FALSE, 0);
  }

  // We use an alignment to control the titlebar height.
  titlebar_alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_box_pack_start(GTK_BOX(container_), titlebar_alignment_, TRUE,
                     TRUE, 0);

  // Put the tab strip in the titlebar.
  gtk_container_add(GTK_CONTAINER(titlebar_alignment_),
                    browser_window_->tabstrip()->widget());

  // We put the min/max/restore/close buttons in a vbox so they are top aligned
  // and don't vertically stretch.
  titlebar_buttons_box_ = gtk_vbox_new(FALSE, 0);
  GtkWidget* buttons_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(titlebar_buttons_box_), buttons_hbox, FALSE,
                     FALSE, 0);

  close_button_.reset(BuildTitlebarButton(IDR_CLOSE, IDR_CLOSE_P, IDR_CLOSE_H,
                      buttons_hbox, IDS_XPFRAME_CLOSE_TOOLTIP));
  restore_button_.reset(BuildTitlebarButton(IDR_RESTORE, IDR_RESTORE_P,
                        IDR_RESTORE_H, buttons_hbox,
                        IDS_XPFRAME_RESTORE_TOOLTIP));
  maximize_button_.reset(BuildTitlebarButton(IDR_MAXIMIZE, IDR_MAXIMIZE_P,
                         IDR_MAXIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MAXIMIZE_TOOLTIP));
  minimize_button_.reset(BuildTitlebarButton(IDR_MINIMIZE, IDR_MINIMIZE_P,
                         IDR_MINIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MINIMIZE_TOOLTIP));

  gtk_box_pack_end(GTK_BOX(container_), titlebar_buttons_box_, FALSE,
                   FALSE, 0);

  gtk_widget_show_all(container_);
}

CustomDrawButton* BrowserTitlebar::BuildTitlebarButton(int image,
    int image_pressed, int image_hot, GtkWidget* box, int tooltip) {
  CustomDrawButton* button = new CustomDrawButton(image, image_pressed,
      image_hot, 0);
  gtk_widget_add_events(GTK_WIDGET(button->widget()), GDK_POINTER_MOTION_MASK);
  g_signal_connect(button->widget(), "clicked", G_CALLBACK(OnButtonClicked),
                   this);
  g_signal_connect(button->widget(), "motion-notify-event",
                   G_CALLBACK(OnMouseMoveEvent), browser_window_);
  std::string localized_tooltip = l10n_util::GetStringUTF8(tooltip);
  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  gtk_box_pack_end(GTK_BOX(box), button->widget(), FALSE, FALSE, 0);
  return button;
}

void BrowserTitlebar::UpdateCustomFrame(bool use_custom_frame) {
  using_custom_frame_ = use_custom_frame;
  if (use_custom_frame)
    gtk_widget_show(titlebar_buttons_box_);
  else
    gtk_widget_hide(titlebar_buttons_box_);
  UpdateTitlebarAlignment();
}

void BrowserTitlebar::UpdateTitlebarAlignment() {
  if (using_custom_frame_ && !browser_window_->IsMaximized()) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_),
        kTitlebarHeight, 0, 0, 0);
  } else {
    gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_), 0, 0, 0, 0);
  }
}

// static
gboolean BrowserTitlebar::OnWindowStateChanged(GtkWindow* window,
    GdkEventWindowState* event, BrowserTitlebar* titlebar) {
  // Update the maximize/restore button.
  if (titlebar->browser_window_->IsMaximized()) {
    gtk_widget_hide(titlebar->maximize_button_->widget());
    gtk_widget_show(titlebar->restore_button_->widget());
  } else {
    gtk_widget_hide(titlebar->restore_button_->widget());
    gtk_widget_show(titlebar->maximize_button_->widget());
  }
  titlebar->UpdateTitlebarAlignment();
  return FALSE;
}

// static
void BrowserTitlebar::OnButtonClicked(GtkWidget* button,
                                      BrowserTitlebar* titlebar) {
  if (titlebar->close_button_->widget() == button) {
    titlebar->browser_window_->Close();
  } else if (titlebar->restore_button_->widget() == button) {
    gtk_window_unmaximize(titlebar->window_);
  } else if (titlebar->maximize_button_->widget() == button) {
    gtk_window_maximize(titlebar->window_);
  } else if (titlebar->minimize_button_->widget() == button) {
    gtk_window_iconify(titlebar->window_);
  }
}

void BrowserTitlebar::ShowContextMenu() {
  if (!context_menu_.get()) {
    context_menu_.reset(new MenuGtk(this, false));
    context_menu_->AppendMenuItemWithLabel(
        IDC_NEW_TAB,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_NEWTAB));
    context_menu_->AppendMenuItemWithLabel(
        IDC_RESTORE_TAB,
        l10n_util::GetStringUTF8(IDS_RESTORE_TAB));

    context_menu_->AppendSeparator();

    context_menu_->AppendMenuItemWithLabel(
        IDC_TASK_MANAGER,
        l10n_util::GetStringUTF8(IDS_TASK_MANAGER));

    context_menu_->AppendSeparator();

    context_menu_->AppendCheckMenuItemWithLabel(
        kShowWindowDecorationsCommand,
        l10n_util::GetStringUTF8(IDS_SHOW_WINDOW_DECORATIONS));
  }

  context_menu_->PopupAsContext(gtk_get_current_event_time());
}

bool BrowserTitlebar::IsCommandEnabled(int command_id) const {
  switch (command_id) {
    case IDC_NEW_TAB:
    case kShowWindowDecorationsCommand:
      return true;

    case IDC_RESTORE_TAB:
      return browser_window_->browser()->CanRestoreTab();

    case IDC_TASK_MANAGER:
      // TODO(tc): Task manager needs to be implemented.
      return false;

    default:
      NOTREACHED();
  }
  return false;
}

bool BrowserTitlebar::IsItemChecked(int command_id) const {
  DCHECK(command_id == kShowWindowDecorationsCommand);
  PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
  return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
}

void BrowserTitlebar::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDC_NEW_TAB:
    case IDC_RESTORE_TAB:
    case IDC_TASK_MANAGER:
      browser_window_->browser()->ExecuteCommand(command_id);
      break;

    case kShowWindowDecorationsCommand:
    {
      PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }

    default:
      NOTREACHED();
  }
}
