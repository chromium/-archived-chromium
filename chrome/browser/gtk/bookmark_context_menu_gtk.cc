// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_context_menu_gtk.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(BookmarkNode* node) {
  if (node->is_url())
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

}  // namespace

BookmarkContextMenuGtk::BookmarkContextMenuGtk(
    GtkWindow* window,
    Profile* profile,
    Browser* browser,
    PageNavigator* navigator,
    BookmarkNode* parent,
    const std::vector<BookmarkNode*>& selection,
    ConfigurationType configuration)
    : window_(window),
      profile_(profile),
      browser_(browser),
      navigator_(navigator),
      parent_(parent),
      selection_(selection),
      model_(profile->GetBookmarkModel()),
      configuration_(configuration),
      menu_(new MenuGtk(this, false)) {
  if (configuration != BOOKMARK_MANAGER_ORGANIZE_MENU) {
    if (selection.size() == 1 && selection[0]->is_url()) {
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL, IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB);
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
                 IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW);
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
                 IDS_BOOMARK_BAR_OPEN_INCOGNITO);
    } else {
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL, IDS_BOOMARK_BAR_OPEN_ALL);
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
                 IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW);
      AppendItem(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
                 IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO);
    }
    AppendSeparator();
  }

  if (selection.size() == 1 && selection[0]->is_folder()) {
    AppendItem(IDS_BOOKMARK_BAR_RENAME_FOLDER);
  } else {
    AppendItem(IDS_BOOKMARK_BAR_EDIT);
  }
  AppendItem(IDS_BOOKMARK_BAR_REMOVE);

  if (configuration == BOOKMARK_MANAGER_TABLE ||
      configuration == BOOKMARK_MANAGER_TABLE_OTHER ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) {
    AppendItem(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER);
  }

  if (configuration == BOOKMARK_MANAGER_TABLE ||
      configuration == BOOKMARK_MANAGER_TABLE_OTHER ||
      configuration == BOOKMARK_MANAGER_TREE ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) {
    AppendSeparator();
    AppendItem(IDS_CUT);
    AppendItem(IDS_COPY);
    AppendItem(IDS_PASTE);
  }

  if (configuration == BOOKMARK_MANAGER_ORGANIZE_MENU) {
    AppendSeparator();
    AppendItem(IDS_BOOKMARK_MANAGER_SORT);
  }

  AppendSeparator();

  AppendItem(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK);
  AppendItem(IDS_BOOMARK_BAR_NEW_FOLDER);

  if (configuration == BOOKMARK_BAR) {
    AppendSeparator();
    AppendItem(IDS_BOOKMARK_MANAGER);
    AppendItem(IDS_BOOMARK_BAR_ALWAYS_SHOW);
  }

  model_->AddObserver(this);
}

BookmarkContextMenuGtk::~BookmarkContextMenuGtk() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenuGtk::PopupAsContext(guint32 event_time) {
  menu_->PopupAsContext(event_time);
}

bool BookmarkContextMenuGtk::IsCommandEnabled(int index) const {
  bool is_root_node =
      (selection_.size() == 1 &&
       selection_[0]->GetParent() == model_->root_node());
  switch (index) {
    case IDS_BOOMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
      return HasURLs() && !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return HasURLs();

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node;

    case IDS_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      return (configuration_ == BOOKMARK_MANAGER_TABLE_OTHER ||
              configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) &&
             selection_.size() == 1;

    case IDS_BOOKMARK_MANAGER_SORT:
      return parent_ && parent_ != model_->root_node();

    case IDS_BOOMARK_BAR_NEW_FOLDER:
    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK:
      return GetParentForNewNodes() != NULL;

    case IDS_COPY:
    case IDS_CUT:
      return selection_.size() > 0 && !is_root_node;

    // TODO(erg): Port boomark_utils::CanPasteFromClipboard
    // case IDS_PASTE:
    //   // Always paste to parent.
    //   return bookmark_utils::CanPasteFromClipboard(parent_);
  }
  return true;
}

void BookmarkContextMenuGtk::ExecuteCommand(int index) {
  switch (index) {
    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      PageNavigator* navigator = browser_ ?
          browser_->GetSelectedTabContents() : navigator_;
      WindowOpenDisposition initial_disposition;
      if (index == IDS_BOOMARK_BAR_OPEN_ALL) {
        initial_disposition = NEW_FOREGROUND_TAB;
        UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenAll",
                                  profile_);
      } else if (index == IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = NEW_WINDOW;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllInNewWindow", profile_);
      } else {
        initial_disposition = OFF_THE_RECORD;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllIncognito", profile_);
      }

      bookmark_utils::OpenAll(window_, profile_, navigator,
                              selection_, initial_disposition);
      break;
    }

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Edit", profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      if (selection_[0]->is_url()) {
        NOTIMPLEMENTED() << "Bookmark editor not implemented";
      } else {
        NOTIMPLEMENTED() << "Folder editor not implemented";
      }
      break;
    }

    case IDS_BOOKMARK_BAR_REMOVE: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Remove", profile_);
      BookmarkModel* model = RemoveModelObserver();

      for (size_t i = 0; i < selection_.size(); ++i) {
        model->Remove(selection_[i]->GetParent(),
                      selection_[i]->GetParent()->IndexOfChild(selection_[i]));
      }
      selection_.clear();
      break;
    }

    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Add", profile_);
      NOTIMPLEMENTED() << "Adding new bookmark not implemented";
      break;
    }

    case IDS_BOOMARK_BAR_NEW_FOLDER: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_NewFolder",
                                profile_);

      NOTIMPLEMENTED() << "EditFolderController not implemented";
      break;
    }

    case IDS_BOOMARK_BAR_ALWAYS_SHOW: {
      bookmark_utils::ToggleWhenVisible(profile_);
      break;
    }

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_ShowInFolder",
                                profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      NOTIMPLEMENTED() << "Bookmark Manager not implemented";
      break;
    }

    case IDS_BOOKMARK_MANAGER: {
      UserMetrics::RecordAction(L"ShowBookmarkManager", profile_);
      NOTIMPLEMENTED() << "Bookmark Manager not implemented";
      break;
    }

    case IDS_BOOKMARK_MANAGER_SORT: {
      UserMetrics::RecordAction(L"BookmarkManager_Sort", profile_);
      model_->SortChildren(parent_);
      break;
    }

    case IDS_COPY:
    case IDS_CUT:
    case IDS_PASTE: {
      NOTIMPLEMENTED() << "Cut/Copy/Paste not implemented";
      break;
    }

    default:
      NOTREACHED();
  }
}

void BookmarkContextMenuGtk::AppendItem(int id) {
  menu_->AppendMenuItemWithLabel(
      id,
      MenuGtk::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(id)));
}

void BookmarkContextMenuGtk::AppendItem(int id, int localization_id) {
  menu_->AppendMenuItemWithLabel(
      id,
      MenuGtk::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(localization_id)));
}

void BookmarkContextMenuGtk::AppendSeparator() {
  menu_->AppendSeparator();
}

void BookmarkContextMenuGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  ModelChanged();
}

void BookmarkContextMenuGtk::BookmarkNodeMoved(BookmarkModel* model,
                                            BookmarkNode* old_parent,
                                            int old_index,
                                            BookmarkNode* new_parent,
                                            int new_index) {
  ModelChanged();
}

void BookmarkContextMenuGtk::BookmarkNodeAdded(BookmarkModel* model,
                                            BookmarkNode* parent,
                                            int index) {
  ModelChanged();
}

void BookmarkContextMenuGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                              BookmarkNode* parent,
                                              int index,
                                              BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuGtk::BookmarkNodeChanged(BookmarkModel* model,
                                              BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuGtk::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                        BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuGtk::ModelChanged() {
  menu_->Cancel();
}

BookmarkModel* BookmarkContextMenuGtk::RemoveModelObserver() {
  BookmarkModel* model = model_;
  model_->RemoveObserver(this);
  model_ = NULL;
  return model;
}

bool BookmarkContextMenuGtk::HasURLs() const {
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (NodeHasURLs(selection_[i]))
      return true;
  }
  return false;
}

BookmarkNode* BookmarkContextMenuGtk::GetParentForNewNodes() const {
  return (selection_.size() == 1 && selection_[0]->is_folder()) ?
      selection_[0] : parent_;
}
