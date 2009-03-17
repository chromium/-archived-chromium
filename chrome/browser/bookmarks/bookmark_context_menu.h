// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// TODO(port): Port this file.
#if defined(OS_WIN)
#include "chrome/views/controls/menu/chrome_menu.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Browser;
class PageNavigator;

// BookmarkContextMenu manages the context menu shown for the
// bookmark bar, items on the bookmark bar, submenus of the bookmark bar and
// the bookmark manager.
class BookmarkContextMenu : public views::MenuDelegate,
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

  // Creates the bookmark context menu.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |browser| is used to determine the PageNavigator and may be null.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  // |configuration| determines which items to show.
  BookmarkContextMenu(gfx::NativeWindow hwnd,
                      Profile* profile,
                      Browser* browser,
                      PageNavigator* navigator,
                      BookmarkNode* parent,
                      const std::vector<BookmarkNode*>& selection,
                      ConfigurationType configuration);
  virtual ~BookmarkContextMenu();

  // Shows the menu at the specified place.
  void RunMenuAt(int x, int y);

  // Returns the menu.
  views::MenuItemView* menu() const { return menu_.get(); }

  // Menu::Delegate methods.
  virtual void ExecuteCommand(int id);
  virtual bool IsItemChecked(int id) const;
  virtual bool IsCommandEnabled(int id) const;

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

  gfx::NativeWindow wnd_;
  Profile* profile_;
  Browser* browser_;
  PageNavigator* navigator_;
  BookmarkNode* parent_;
  std::vector<BookmarkNode*> selection_;
  scoped_ptr<views::MenuItemView> menu_;
  BookmarkModel* model_;
  ConfigurationType configuration_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenu);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
