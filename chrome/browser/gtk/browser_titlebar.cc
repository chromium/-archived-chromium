// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_titlebar.h"

#include <gtk/gtk.h>

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

}

BrowserTitlebar::BrowserTitlebar(BrowserWindowGtk* browser_window,
                                 GtkWindow* window)
    : browser_window_(browser_window), window_(window) {
  Init();
}

void BrowserTitlebar::Init() {
  titlebar_background_.reset(new NineBox(
      browser_window_->browser()->profile()->GetThemeProvider(),
      0, IDR_THEME_FRAME, 0, 0, 0, 0, 0, 0, 0));
  titlebar_background_otr_.reset(new NineBox(
      browser_window_->browser()->profile()->GetThemeProvider(),
      0, IDR_THEME_FRAME_INCOGNITO, 0, 0, 0, 0, 0, 0, 0));

  // The widget hierarchy is shown below.  In addition to the diagram, there is
  // a gtk event box surrounding the titlebar_hbox which catches mouse events
  // in the titlebar.
  //
  // +- HBox (titlebar_hbox) -----------------------------------------------+
  // |+- Alignment (titlebar_alignment_)-++- VBox (titlebar_buttons_box_) -+|
  // ||                                  ||+- HBox -----------------------+||
  // ||                                  |||+- button -++- button -+      |||
  // ||+- TabStripGtk ------------------+|||| minimize || restore  | ...  |||
  // ||| tab   tab   tab    tabclose    +|||+----------++----------+      |||
  // ||+--------------------------------+||+------------------------------+||
  // |+----------------------------------++--------------------------------+|
  // +----------------------------------------------------------------------+
  container_ = gtk_event_box_new();
  GtkWidget* titlebar_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(container_), titlebar_hbox);

  g_signal_connect(G_OBJECT(container_), "button-press-event",
                   G_CALLBACK(OnMouseButtonPress), this);
  g_signal_connect(G_OBJECT(titlebar_hbox), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateChanged), this);

  // We use an alignment to control the titlebar height.
  titlebar_alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_box_pack_start(GTK_BOX(titlebar_hbox), titlebar_alignment_, TRUE,
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

  gtk_box_pack_end(GTK_BOX(titlebar_hbox), titlebar_buttons_box_, FALSE,
                   FALSE, 0);

  gtk_widget_show_all(container_);
}

CustomDrawButton* BrowserTitlebar::BuildTitlebarButton(int image,
    int image_pressed, int image_hot, GtkWidget* box, int tooltip) {
  CustomDrawButton* button = new CustomDrawButton(image, image_pressed,
      image_hot, 0);
  g_signal_connect(button->widget(), "clicked", G_CALLBACK(OnButtonClicked),
                   this);
  std::string localized_tooltip = l10n_util::GetStringUTF8(tooltip);
  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  gtk_box_pack_end(GTK_BOX(box), button->widget(), FALSE, FALSE, 0);
  return button;
}

void BrowserTitlebar::UpdateCustomFrame(bool use_custom_frame) {
  if (use_custom_frame) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_),
        kTitlebarHeight, 0, 0, 0);
    gtk_widget_show(titlebar_buttons_box_);
  } else {
    gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_), 0, 0, 0, 0);
    gtk_widget_hide(titlebar_buttons_box_);
  }
}

gboolean BrowserTitlebar::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                   BrowserTitlebar* titlebar) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, e->area.x, e->area.y, e->area.width, e->area.height);
  cairo_clip(cr);
  Profile* profile = titlebar->browser_window_->browser()->profile();
  NineBox* image = profile->IsOffTheRecord()
      ? titlebar->titlebar_background_otr_.get()
      : titlebar->titlebar_background_.get();
  image->RenderTopCenterStrip(cr, e->area.x, 0, e->area.width);
  cairo_destroy(cr);

  return FALSE;  // Allow subwidgets to paint.
}

gboolean BrowserTitlebar::OnMouseButtonPress(GtkWidget* widget,
    GdkEventButton* event, BrowserTitlebar* titlebar) {
  if (1 == event->button) {
    if (GDK_BUTTON_PRESS == event->type) {
      gtk_window_begin_move_drag(GTK_WINDOW(titlebar->window_),
          event->button, event->x_root, event->y_root, event->time);
      return TRUE;
    } else if (GDK_2BUTTON_PRESS == event->type) {
      // Maximize/restore on double click.
      if (titlebar->browser_window_->IsMaximized()) {
        gtk_window_unmaximize(titlebar->window_);
      } else {
        gtk_window_maximize(titlebar->window_);
      }
      return TRUE;
    }
  } else if (3 == event->button) {
    titlebar->ShowContextMenu();
    return TRUE;
  }

  return FALSE;  // Continue to propagate the event.
}

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
  return FALSE;
}

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
