// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_H_

#include <map>

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/views/controls/menu/chrome_menu.h"

class BookmarkContextMenu;
class BookmarkNode;
class Browser;
class OSExchangeData;
class PageNavigator;
class Profile;

// BookmarkMenuController is responsible for showing a menu of bookmarks,
// each item in the menu represents a bookmark.
// BookmarkMenuController deletes itself as necessary, although the menu can
// be explicitly hidden by way of the Cancel method.
class BookmarkMenuController : public BaseBookmarkModelObserver,
                               public views::MenuDelegate {
 public:
  // The observer is notified prior to the menu being deleted.
  class Observer {
   public:
    virtual void BookmarkMenuDeleted(BookmarkMenuController* controller) = 0;
  };

  // Creates a BookmarkMenuController showing the children of |node| starting
  // at index |start_child_index|.
  BookmarkMenuController(Browser* browser,
                         Profile* profile,
                         PageNavigator* page_navigator,
                         HWND hwnd,
                         BookmarkNode* node,
                         int start_child_index,
                         bool show_other_folder);

  // Shows the menu.
  void RunMenuAt(const gfx::Rect& bounds,
                 views::MenuItemView::AnchorPosition position,
                 bool for_drop);

  // Hides the menu.
  void Cancel();

  // Returns the node the menu is showing for.
  BookmarkNode* node() const { return node_; }

  // Returns the menu.
  views::MenuItemView* menu() const { return menu_.get(); }

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() const {
    return context_menu_.get() ? context_menu_->menu() : NULL;
  }

  void set_observer(Observer* observer) { observer_ = observer; }

  // MenuDelegate methods.
  virtual bool IsTriggerableEvent(const views::MouseEvent& e);
  virtual void ExecuteCommand(int id, int mouse_event_flags);
  virtual bool CanDrop(views::MenuItemView* menu, const OSExchangeData& data);
  virtual int GetDropOperation(views::MenuItemView* item,
                               const views::DropTargetEvent& event,
                               DropPosition* position);
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const views::DropTargetEvent& event);
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               int x,
                               int y,
                               bool is_mouse_gesture);
  virtual void DropMenuClosed(views::MenuItemView* menu);
  virtual bool CanDrag(views::MenuItemView* menu);
  virtual void WriteDragData(views::MenuItemView* sender, OSExchangeData* data);
  virtual int GetDragOperations(views::MenuItemView* sender);

  virtual void BookmarkModelChanged();
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node);

 private:
  // BookmarkMenuController deletes itself as necessary.
  ~BookmarkMenuController();

  // Builds the menu for the other bookmarks folder. This is added as the last
  // item to menu_.
  void BuildOtherFolderMenu(int* next_menu_id);

  // Creates an entry in menu for each child node of |parent| starting at
  // |start_child_index|.
  void BuildMenu(BookmarkNode* parent,
                 int start_child_index,
                 views::MenuItemView* menu,
                 int* next_menu_id);

  Browser* browser_;

  Profile* profile_;

  PageNavigator* page_navigator_;

  // Parent of menus.
  HWND hwnd_;

  // The node we're showing the contents of.
  BookmarkNode* node_;

  // Maps from menu id to BookmarkNode.
  std::map<int, BookmarkNode*> menu_id_to_node_map_;

  // Mapping from node to menu id. This only contains entries for nodes of type
  // URL.
  std::map<BookmarkNode*, int> node_to_menu_id_map_;

  // The menu.
  scoped_ptr<views::MenuItemView> menu_;

  // Data for the drop.
  BookmarkDragData drop_data_;

  // Used when a context menu is shown.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  // The observer, may be null.
  Observer* observer_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // Should the other folder be shown?
  bool show_other_folder_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_H_
