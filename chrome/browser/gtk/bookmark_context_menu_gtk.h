// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/gtk/menu_gtk.h"

class Profile;
class Browser;
class PageNavigator;

typedef struct _GtkWindow GtkWindow;

// The context menu that opens or modifies bookmarks. (This is not the menu
// that displays folders contents.)
//
// TODO(erg): This is a copy of
// ./browser/bookmarks/bookmark_context_menu.{cc,h} and should be merged with
// that file once it is sufficiently de-views-ed.
class BookmarkContextMenuGtk : public MenuGtk::Delegate,
                               public BookmarkModelObserver {
 public:
  // Used to configure what the context menu shows.
  enum ConfigurationType {
    BOOKMARK_BAR,
    BOOKMARK_MANAGER_TABLE,
    // Used when the source is the table in the bookmark manager and the table
    // is showing recently bookmarked or searched.
    BOOKMARK_MANAGER_TABLE_OTHER,
    BOOKMARK_MANAGER_TREE,
    BOOKMARK_MANAGER_ORGANIZE_MENU,
    // Used when the source is the bookmark manager and the table is showing
    // recently bookmarked or searched.
    BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER
  };

  BookmarkContextMenuGtk(GtkWindow* window,
                         Profile* profile,
                         Browser* browser,
                         PageNavigator* navigator,
                         BookmarkNode* parent,
                         const std::vector<BookmarkNode*>& selection,
                         ConfigurationType configuration);
  virtual ~BookmarkContextMenuGtk();

  // Pops up this menu.
  void PopupAsContext(guint32 event_time);

  // Overridden from MenuGtk::Delegate:
  virtual bool IsCommandEnabled(int index) const;
  virtual void ExecuteCommand(int index);

 private:
  // BookmarkModelObserver method. Any change to the model results in closing
  // the menu.
  virtual void Loaded(BookmarkModel* model) {}
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index,
                                   BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node);

  // Adds a IDS_* style command to the menu.
  void AppendItem(int id);
  // Adds a IDS_* style command to the menu with a different localized string.
  void AppendItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AppendSeparator();

  // Invoked from the various bookmark model observer methods. Closes the menu.
  void ModelChanged();

  // Removes the observer from the model and NULLs out model_.
  BookmarkModel* RemoveModelObserver();

  // Returns true if selection_ has at least one bookmark of type url.
  bool HasURLs() const;

  // Returns the parent for newly created folders/bookmarks. If selection_
  // has one element and it is a folder, selection_[0] is returned, otherwise
  // parent_ is returned.
  BookmarkNode* GetParentForNewNodes() const;

  gfx::NativeWindow window_;
  Profile* profile_;
  Browser* browser_;
  PageNavigator* navigator_;
  BookmarkNode* parent_;
  std::vector<BookmarkNode*> selection_;
  BookmarkModel* model_;
  ConfigurationType configuration_;

  scoped_ptr<MenuGtk> menu_;
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_
