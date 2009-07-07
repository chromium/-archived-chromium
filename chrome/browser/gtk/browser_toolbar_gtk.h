// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/pref_member.h"

class BackForwardButtonGtk;
class Browser;
class BrowserWindowGtk;
class CustomDrawButton;
class GoButtonGtk;
class LocationBar;
class LocationBarViewGtk;
class NineBox;
class Profile;
class TabContents;
class ToolbarModel;
class ToolbarStarToggleGtk;

// View class that displays the GTK version of the toolbar and routes gtk
// events back to the Browser.
class BrowserToolbarGtk : public CommandUpdater::CommandObserver,
                          public MenuGtk::Delegate,
                          public NotificationObserver,
                          public AutocompletePopupPositioner {
 public:
  explicit BrowserToolbarGtk(Browser* browser, BrowserWindowGtk* window);
  virtual ~BrowserToolbarGtk();

  // Create the contents of the toolbar. |top_level_window| is the GtkWindow
  // to which we attach our accelerators.
  void Init(Profile* profile, GtkWindow* top_level_window);

  // Adds this GTK toolbar into a sizing box.
  void AddToolbarToBox(GtkWidget* box);

  void Show();
  void Hide();

  // Getter for the containing widget.
  GtkWidget* widget() {
    return toolbar_;
  }

  virtual LocationBar* GetLocationBar() const;

  GoButtonGtk* GetGoButton() { return go_.get(); }

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from MenuGtk::Delegate:
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int command_id);
  virtual void StoppedShowing();

  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  Profile* profile() { return profile_; }
  void SetProfile(Profile* profile);

  // Message that we should react to a state change.
  void UpdateTabContents(TabContents* contents, bool should_restore_state);

  ToolbarStarToggleGtk* star() { return star_.get(); }

  // Alerts us that the theme changed, and we might need to change theme images.
  void UserChangedTheme();

  // Implement AutocompletePopupPositioner, return the position of where the
  // Omnibox results popup should go (from the star to the go buttons).
  virtual gfx::Rect GetPopupBounds() const;

 private:
  // Builds a toolbar button with all the properties set.
  // |spacing| is the width of padding (in pixels) on the left and right of the
  // button.
  CustomDrawButton* BuildToolbarButton(int normal_id,
                                       int active_id,
                                       int highlight_id,
                                       int depressed_id,
                                       const std::string& localized_tooltip,
                                       const char* stock_id);

  // Create the star button given the tooltip.  Returns the widget created.
  ToolbarStarToggleGtk* BuildStarButton(const std::string& localized_tooltip);

  // Create a menu for the toolbar given the icon id and tooltip.  Returns the
  // widget created.
  GtkWidget* BuildToolbarMenuButton(
      int icon_id,
      const std::string& localized_tooltip,
      OwnedWidgetGtk* owner);

  // Connect signals for dragging a url onto the home button.
  void SetUpDragForHomeButton();

  // Helper for the PageAppMenu event handlers. Pops down the currently active
  // meun and pops up the other menu.
  void ChangeActiveMenu(GtkWidget* active_menu, guint timestamp);

  // Gtk callback for the "expose-event" signal.
  static gboolean OnToolbarExpose(GtkWidget* widget, GdkEventExpose* e,
                                  BrowserToolbarGtk* toolbar);

  // Gtk callback for the "clicked" signal.
  static void OnButtonClick(GtkWidget* button, BrowserToolbarGtk* toolbar);

  // Gtk callback for the "button-release-event" signal.
  static gboolean OnButtonRelease(GtkWidget* button, GdkEventButton* event,
                                  BrowserToolbarGtk* toolbar);

  // Gtk callback to intercept mouse clicks to the menu buttons.
  static gboolean OnMenuButtonPressEvent(GtkWidget* button,
                                         GdkEventButton* event,
                                         BrowserToolbarGtk* toolbar);

  // Used for drags onto home button.
  static void OnDragDataReceived(GtkWidget* widget,
                                 GdkDragContext* drag_context,
                                 gint x, gint y,
                                 GtkSelectionData* data,
                                 guint info, guint time,
                                 BrowserToolbarGtk* toolbar);

  // These event handlers are used to fake menu-bar behavior in the page and
  // app menus.
  static gboolean OnPageAppMenuMouseMotion(GtkWidget* widget,
                                           GdkEventMotion* event,
                                           BrowserToolbarGtk* toolbar);
  static void OnPageAppMenuMoveCurrent(GtkWidget* widget,
                                       GtkMenuDirectionType dir,
                                       BrowserToolbarGtk* toolbar);

  // Initialize the background NineBox.
  void InitNineBox();

  // Ninebox for the toolbar background
  scoped_ptr<NineBox> background_ninebox_;

  // Gtk widgets. The toolbar is an hbox with each of the other pieces of the
  // toolbar placed side by side.
  GtkWidget* toolbar_;

  // The location bar view.
  scoped_ptr<LocationBarViewGtk> location_bar_;

  // A pointer to our window's accelerator list.
  GtkAccelGroup* accel_group_;

  // All the buttons in the toolbar.
  scoped_ptr<BackForwardButtonGtk> back_, forward_;
  scoped_ptr<CustomDrawButton> reload_;
  scoped_ptr<CustomDrawButton> home_;
  scoped_ptr<ToolbarStarToggleGtk> star_;
  scoped_ptr<GoButtonGtk> go_;
  OwnedWidgetGtk page_menu_button_, app_menu_button_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  scoped_ptr<MenuGtk> page_menu_;
  scoped_ptr<MenuGtk> app_menu_;

  Browser* browser_;
  BrowserWindowGtk* window_;
  Profile* profile_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The event state the last time we observed a button release event.
  int last_release_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(BrowserToolbarGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_TOOLBAR_GTK_H_
