// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_GTK_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_GTK_H_

#include <map>

#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/gtk/menu_gtk.h"

class BookmarkContextMenu;
class Browser;
class Profiler;
class PageNavigator;
class BookmarkModel;
class BookmarkNode;

typedef struct _GtkWidget GtkWidget;

class BookmarkMenuController : public BaseBookmarkModelObserver,
                               public MenuGtk::Delegate {
 public:
  // Creates a BookmarkMenuController showing the children of |node| starting
  // at index |start_child_index|.
  BookmarkMenuController(Browser* browser,
                         Profile* profile,
                         PageNavigator* page_navigator,
                         GtkWindow* window,
                         BookmarkNode* node,
                         int start_child_index,
                         bool show_other_folder);
  virtual ~BookmarkMenuController();

  void Popup(GtkWidget* widget, gint button_type, guint32 timestamp);

  // Overridden from BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged();
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node);

  // Overridden from MenuGtk::Delegate:
  virtual bool IsCommandEnabled(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  void BuildMenu(BookmarkNode* parent,
                 int start_child_index,
                 MenuGtk* menu,
                 int* next_menu_id);

  Browser* browser_;
  Profile* profile_;
  PageNavigator* page_navigator_;

  // Parent window of this menu.
  GtkWindow* parent_window_;

  // The node we're showing the contents of.
  BookmarkNode* node_;

  scoped_ptr<MenuGtk> menu_;

  // Maps from menu id to BookmarkNode.
  std::map<int, BookmarkNode*> menu_id_to_node_map_;

  // Mapping from node to menu id. This only contains entries for nodes of type
  // URL.
  std::map<BookmarkNode*, int> node_to_menu_id_map_;

  // Used when a context menu is shown.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_GTK_H_
