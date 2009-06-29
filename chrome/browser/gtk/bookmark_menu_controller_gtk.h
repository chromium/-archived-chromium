// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_MENU_CONTROLLER_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_MENU_CONTROLLER_GTK_H_

#include <gtk/gtk.h>

#include <map>

#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/common/owned_widget_gtk.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkContextMenu;
class Browser;
class Profiler;
class PageNavigator;
class BookmarkModel;
class BookmarkNode;

class BookmarkMenuController : public BaseBookmarkModelObserver {
 public:
  // Creates a BookmarkMenuController showing the children of |node| starting
  // at index |start_child_index|.
  BookmarkMenuController(Browser* browser,
                         Profile* profile,
                         PageNavigator* page_navigator,
                         GtkWindow* window,
                         const BookmarkNode* node,
                         int start_child_index,
                         bool show_other_folder);
  virtual ~BookmarkMenuController();

  // Pops up the menu.
  void Popup(GtkWidget* widget, gint button_type, guint32 timestamp);

  // Overridden from BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged();
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);

 private:
  // Recursively change the bookmark hierarchy rooted in |parent| into a set of
  // gtk menus rooted in |menu|.
  void BuildMenu(const BookmarkNode* parent,
                 int start_child_index,
                 GtkWidget* menu);

  // Calls the page navigator to navigate to the node represented by
  // |menu_item|.
  void NavigateToMenuItem(GtkWidget* menu_item,
                          WindowOpenDisposition disposition);

  // Button press and release events for a GtkMenuItem. We have to override
  // these separate from OnMenuItemActivated because we need to handle right
  // clicks and opening bookmarks with different dispositions.
  static gboolean OnButtonPressed(GtkWidget* sender,
                                  GdkEventButton* event,
                                  BookmarkMenuController* controller);
  static gboolean OnButtonReleased(GtkWidget* sender,
                                   GdkEventButton* event,
                                   BookmarkMenuController* controller);

  // We respond to the activate signal because things other than mouse button
  // events can trigger it.
  static void OnMenuItemActivated(GtkMenuItem* menuitem,
                                  BookmarkMenuController* controller);

  // The individual GtkMenuItems in the BookmarkMenu are all drag sources.
  static void OnMenuItemDragBegin(GtkWidget* menu_item,
                                  GdkDragContext* drag_context,
                                  BookmarkMenuController* bar);
  static void OnMenuItemDragEnd(GtkWidget* menu_item,
                                GdkDragContext* drag_context,
                                BookmarkMenuController* controller);
  static void OnMenuItemDragGet(
      GtkWidget* widget, GdkDragContext* context,
      GtkSelectionData* selection_data,
      guint target_type, guint time,
      BookmarkMenuController* controller);

  Browser* browser_;
  Profile* profile_;
  PageNavigator* page_navigator_;

  // Parent window of this menu.
  GtkWindow* parent_window_;

  // The bookmark model.
  BookmarkModel* model_;

  // The node we're showing the contents of.
  const BookmarkNode* node_;

  // Our bookmark menus. We don't use the MenuGtk class because we have to do
  // all sorts of weird non-standard things with this menu, like:
  // - The menu is a drag target
  // - The menu items have context menus.
  OwnedWidgetGtk menu_;

  // Whether we should ignore the next button release event (because we were
  // dragging).
  bool ignore_button_release_;

  // Mapping from node to GtkMenuItem menu id. This only contains entries for
  // nodes of type URL.
  std::map<const BookmarkNode*, GtkWidget*> node_to_menu_widget_map_;

  // Owns our right click context menu.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_MENU_CONTROLLER_GTK_H_
