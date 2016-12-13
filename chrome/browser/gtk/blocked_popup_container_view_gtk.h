// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_GTK_BLOCKED_POPUP_CONTAINER_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_BLOCKED_POPUP_CONTAINER_VIEW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/owned_widget_gtk.h"

class BlockedPopupContainerInternalView;
class CustomDrawButton;
class GtkThemeProperties;
class MenuGtk;
class PrefService;
class Profile;
class TabContents;
class TabContentsViewGtk;
class TextButton;

namespace views {
class ImageButton;
}

// The GTK blocked popup container notification.
class BlockedPopupContainerViewGtk : public BlockedPopupContainerView,
                                     public MenuGtk::Delegate {
 public:
  virtual ~BlockedPopupContainerViewGtk();

  // Returns the Gtk view that currently owns us.
  TabContentsViewGtk* ContainingView();

  // Returns the URL and title for popup |index|, used to construct a string for
  // display.
  void GetURLAndTitleForPopup(size_t index,
                              string16* url,
                              string16* title) const;

  // Notification that the theme has changed at that we should detect new
  // values.
  void UserChangedTheme(GtkThemeProperties* properties);

  GtkWidget* widget() { return container_.get(); }

  // Overridden from BlockedPopupContainerView:
  virtual void SetPosition();
  virtual void ShowView();
  virtual void UpdateLabel();
  virtual void HideView();
  virtual void Destroy();

  // Overridden from MenuGtk::Delegate:
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool IsItemChecked(int command_id) const;
  virtual void ExecuteCommand(int command_id);

 private:
  // For the static constructor BlockedPopupContainerView::Create().
  friend class BlockedPopupContainerView;

  // Creates a container for a certain TabContents.
  explicit BlockedPopupContainerViewGtk(BlockedPopupContainer* container);

  // Builds all the messy GTK stuff.
  void Init();

  // Callbacks for the two buttons in the notification
  static void OnMenuButtonClicked(GtkButton *button,
                                  BlockedPopupContainerViewGtk* container);
  static void OnCloseButtonClicked(GtkButton *button,
                                   BlockedPopupContainerViewGtk* container);

  // Draws |container_| with a custom background.
  static gboolean OnContainerExpose(GtkWidget* widget, GdkEventExpose* event,
                                    BlockedPopupContainerViewGtk* container);

  // Our model; calling the shots.
  BlockedPopupContainer* model_;

  // The top level of our local GTK hierarchy.
  OwnedWidgetGtk container_;

  // The "Blocked Popups: XXX" button.
  GtkWidget* menu_button_;

  // Whether we should let GTK paint the background and the button decorations.
  bool use_gtk_rendering_;

  // Closes the container.
  scoped_ptr<CustomDrawButton> close_button_;

  // The popup menu with options to launch blocked popups.
  scoped_ptr<MenuGtk> launch_menu_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewGtk);
};

#endif  // CHROME_BROWSER_GTK_BLOCKED_POPUP_CONTAINER_VIEW_GTK_H_
