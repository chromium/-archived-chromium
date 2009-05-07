// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_menu_controller_gtk.h"

#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "app/resource_bundle.h"
#include "grit/theme_resources.h"
#include "webkit/glue/window_open_disposition.h"

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               GtkWindow* window,
                                               BookmarkNode* node,
                                               int start_child_index,
                                               bool show_other_folder)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      parent_window_(window),
      node_(node) {
  menu_.reset(new MenuGtk(this, false));
  int next_menu_id = 1;
  BuildMenu(node, start_child_index, menu_.get(), &next_menu_id);
}

BookmarkMenuController::~BookmarkMenuController() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
}

void BookmarkMenuController::Popup(GtkWidget* widget, gint button_type,
                                   guint32 timestamp) {
  profile_->GetBookmarkModel()->AddObserver(this);
  menu_->Popup(widget, button_type, timestamp);
}

void BookmarkMenuController::BookmarkModelChanged() {
  menu_->Cancel();
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                       BookmarkNode* node) {
  if (node_to_menu_id_map_.find(node) != node_to_menu_id_map_.end())
    menu_->SetIcon(node->GetFavIcon(), node_to_menu_id_map_[node]);
}

bool BookmarkMenuController::IsCommandEnabled(int id) const {
  return true;
}

void BookmarkMenuController::ExecuteCommand(int id) {
  DCHECK(page_navigator_);
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  GURL url = menu_id_to_node_map_[id]->GetURL();
  page_navigator_->OpenURL(
      url, GURL(),
      // TODO(erg): Plumb mouse events here so things like shift-click or
      // ctrl-click do the right things.
      //
      // event_utils::DispositionFromEventFlags(mouse_event_flags),
      CURRENT_TAB,
      PageTransition::AUTO_BOOKMARK);
}

void BookmarkMenuController::BuildMenu(BookmarkNode* parent,
                                       int start_child_index,
                                       MenuGtk* menu,
                                       int* next_menu_id) {
  DCHECK(!parent->GetChildCount() ||
         start_child_index < parent->GetChildCount());
  for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
    BookmarkNode* node = parent->GetChild(i);
    int id = *next_menu_id;

    (*next_menu_id)++;
    if (node->is_url()) {
      SkBitmap icon = node->GetFavIcon();
      if (icon.width() == 0) {
        icon = *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_DEFAULT_FAVICON);
      }
      menu->AppendMenuItemWithIcon(id, WideToUTF8(node->GetTitle()), icon);
      node_to_menu_id_map_[node] = id;
    } else if (node->is_folder()) {
      SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
      MenuGtk* submenu =
          menu->AppendSubMenuWithIcon(id, WideToUTF8(node->GetTitle()),
                                      *folder_icon);
      BuildMenu(node, 0, submenu, next_menu_id);
    } else {
      NOTREACHED();
    }
    menu_id_to_node_map_[id] = node;
  }
}
