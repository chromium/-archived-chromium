// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_TOOLBAR_VIEW_GTK_H_
#define CHROME_BROWSER_BROWSER_TOOLBAR_VIEW_GTK_H_

#include "chrome/browser/command_updater.h"
#include "chrome/common/pref_member.h"

#include <gtk/gtk.h>

class Browser;
class Profile;
class ToolbarModel;

// View class that displays the GTK version of the toolbar and routes gtk
// events back to the Browser.
class BrowserToolbarGtk : public CommandUpdater::CommandObserver {
 public:
  explicit BrowserToolbarGtk(Browser* browser);
  virtual ~BrowserToolbarGtk();

  // Create the contents of the toolbar
  void Init(Profile* profile);

  // Adds this GTK toolbar into a sizing box.
  void AddToolbarToBox(GtkWidget* box);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  void SetProfile(Profile* profile);

 private:
  // Builds a GtkButton with all the properties set.
  GtkWidget* BuildToolbarButton(const std::wstring& localized_tooltip);

  // Gtk callback for the "clicked" signal.
  static void ButtonClickCallback(GtkWidget* button,
                                  BrowserToolbarGtk* toolbar);

  // Gtk widgets. The toolbar is an hbox with each of the other pieces of the
  // toolbar placed side by side.
  GtkWidget* toolbar_;

  // Tooltip container for all GTK widgets in this class.
  GtkTooltips* toolbar_tooltips_;

  // All the GTK buttons in the toolbar.
  GtkWidget* back_;
  GtkWidget* forward_;
  GtkWidget* reload_;
  GtkWidget* home_;
  GtkWidget* star_;
  GtkWidget* go_;
  GtkWidget* page_menu_;
  GtkWidget* app_menu_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // TODO(port): Port BackForwardMenuModel
  // scoped_ptr<BackForwardMenuModel> back_menu_model_;
  // scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  Browser* browser_;
  Profile* profile_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;
};

#endif
