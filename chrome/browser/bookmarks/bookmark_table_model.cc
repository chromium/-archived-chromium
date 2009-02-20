// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_table_model.h"

#include <limits>

#include "base/string_util.h"
#include "base/time.h"
#include "base/time_format.h"
#include "grit/theme_resources.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "googleurl/src/gurl.h"

#include "generated_resources.h"

namespace {

// Number of bookmarks shown in recently bookmarked.
const int kRecentlyBookmarkedCount = 50;

// VectorBackedBookmarkTableModel ----------------------------------------------

class VectorBackedBookmarkTableModel : public BookmarkTableModel {
 public:
  explicit VectorBackedBookmarkTableModel(BookmarkModel* model)
      : BookmarkTableModel(model) {
  }

  virtual BookmarkNode* GetNodeForRow(int row) {
    return nodes_[row];
  }

  virtual int RowCount() {
    return static_cast<int>(nodes_.size());
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {
    NotifyObserverOfChange(new_parent->GetChild(new_index));
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {
    NotifyObserverOfChange(node);
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {
    NotifyObserverOfChange(node);
  }

 protected:
  void NotifyObserverOfChange(BookmarkNode* node) {
    if (!observer())
      return;

    int index = IndexOfNode(node);
    if (index != -1)
      observer()->OnItemsChanged(index, 1);
  }

  typedef std::vector<BookmarkNode*> Nodes;
  Nodes& nodes() { return nodes_; }

 private:
  Nodes nodes_;

  DISALLOW_COPY_AND_ASSIGN(VectorBackedBookmarkTableModel);
};

// FolderBookmarkTableModel ----------------------------------------------------

// FolderBookmarkTableModel is a TableModel implementation backed by the
// contents of a bookmark folder. FolderBookmarkTableModel caches the contents
// of the folder so that it can send out the correct events when a bookmark
// node is moved.
class FolderBookmarkTableModel : public VectorBackedBookmarkTableModel {
 public:
  FolderBookmarkTableModel(BookmarkModel* model, BookmarkNode* root_node)
      : VectorBackedBookmarkTableModel(model),
        root_node_(root_node) {
    for (int i = 0; i < root_node->GetChildCount(); ++i)
      nodes().push_back(root_node->GetChild(i));
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {
    if (old_parent == root_node_) {
      nodes().erase(nodes().begin() + old_index);
      if (observer())
        observer()->OnItemsRemoved(old_index, 1);
    }
    if (new_parent == root_node_) {
      nodes().insert(nodes().begin() + new_index,
                     root_node_->GetChild(new_index));
      if (observer())
        observer()->OnItemsAdded(new_index, 1);
    }
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    if (root_node_ == parent) {
      nodes().insert(nodes().begin() + index, parent->GetChild(index));
      if (observer())
        observer()->OnItemsAdded(index, 1);
    }
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index,
                                   BookmarkNode* node) {
    if (root_node_->HasAncestor(node)) {
      // We, or one of our ancestors was removed.
      root_node_ = NULL;
      nodes().clear();
      if (observer())
        observer()->OnModelChanged();
      return;
    }
    if (root_node_ == parent) {
      nodes().erase(nodes().begin() + index);
      if (observer())
        observer()->OnItemsRemoved(index, 1);
    }
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {
    NotifyChanged(node);
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {
    NotifyChanged(node);
  }

 private:
  void NotifyChanged(BookmarkNode* node) {
    if (node->GetParent() == root_node_ && observer())
      observer()->OnItemsChanged(node->GetParent()->IndexOfChild(node), 1);
  }

  // The node we're showing the children of. This is set to NULL if the node
  // (or one of its ancestors) is removed from the model.
  BookmarkNode* root_node_;

  DISALLOW_COPY_AND_ASSIGN(FolderBookmarkTableModel);
};

// RecentlyBookmarkedTableModel ------------------------------------------------

class RecentlyBookmarkedTableModel : public VectorBackedBookmarkTableModel {
 public:
  explicit RecentlyBookmarkedTableModel(BookmarkModel* model)
      : VectorBackedBookmarkTableModel(model) {
    UpdateRecentlyBookmarked();
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    if (parent->GetChild(index)->is_url())
      UpdateRecentlyBookmarked();
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int old_index,
                                   BookmarkNode* old_node) {
    if (old_node->is_url())
      UpdateRecentlyBookmarked();
  }

 private:
  void UpdateRecentlyBookmarked() {
    nodes().clear();
    bookmark_utils::GetMostRecentlyAddedEntries(model(),
                                                kRecentlyBookmarkedCount,
                                                &nodes());
    if (observer())
      observer()->OnModelChanged();
  }

  DISALLOW_COPY_AND_ASSIGN(RecentlyBookmarkedTableModel);
};

// BookmarkSearchTableModel ----------------------------------------------------

class BookmarkSearchTableModel : public VectorBackedBookmarkTableModel {
 public:
  BookmarkSearchTableModel(BookmarkModel* model,
                           const std::wstring& search_text)
      : VectorBackedBookmarkTableModel(model),
        search_text_(search_text) {
    bookmark_utils::GetBookmarksContainingText(
        model, search_text, std::numeric_limits<int>::max(), &nodes());
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    BookmarkNode* node = parent->GetChild(index);
    if (bookmark_utils::DoesBookmarkContainText(node, search_text_)) {
      nodes().push_back(node);
      if (observer())
        observer()->OnItemsAdded(static_cast<int>(nodes().size() - 1), 1);
    }
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index,
                                   BookmarkNode* node) {
    int internal_index = IndexOfNode(node);
    if (internal_index == -1)
      return;

    nodes().erase(nodes().begin() + static_cast<int>(internal_index));
    if (observer())
      observer()->OnItemsRemoved(internal_index, 1);
  }

 private:
  const std::wstring search_text_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSearchTableModel);
};

}  // namespace

// BookmarkTableModel ----------------------------------------------------------

// static
BookmarkTableModel* BookmarkTableModel::CreateRecentlyBookmarkedModel(
    BookmarkModel* model) {
  return new RecentlyBookmarkedTableModel(model);
}

// static
BookmarkTableModel* BookmarkTableModel::CreateBookmarkTableModelForFolder(
    BookmarkModel* model, BookmarkNode* node) {
  return new FolderBookmarkTableModel(model, node);
}

// static
BookmarkTableModel* BookmarkTableModel::CreateSearchTableModel(
    BookmarkModel* model,
    const std::wstring& text) {
  return new BookmarkSearchTableModel(model, text);        
}

BookmarkTableModel::BookmarkTableModel(BookmarkModel* model)
    : model_(model),
      observer_(NULL) {
  model_->AddObserver(this);
}

BookmarkTableModel::~BookmarkTableModel() {
  if (model_)
    model_->RemoveObserver(this);
}

std::wstring BookmarkTableModel::GetText(int row, int column_id) {
  BookmarkNode* node = GetNodeForRow(row);
  switch (column_id) {
    case IDS_BOOKMARK_TABLE_TITLE: {
      std::wstring title = node->GetTitle();
      // Adjust the text as well, for example, put LRE-PDF pair around LTR text
      // in RTL enviroment, so that the ending punctuation in the text will not
      // be rendered incorrectly (such as rendered as the leftmost character,
      // and/or rendered as a mirrored punctuation character).
      //
      // TODO(xji): Consider adding a special case if the title text is a URL,
      // since those should always be displayed LTR. Please refer to
      // http://crbug.com/6726 for more information.
      l10n_util::AdjustStringForLocaleDirection(title, &title);
      return title;
    }

    case IDS_BOOKMARK_TABLE_URL: {
      if (!node->is_url())
        return std::wstring();
      std::wstring url_text = UTF8ToWide(node->GetURL().spec());
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
        l10n_util::WrapStringWithLTRFormatting(&url_text);
      return url_text;
    }

    case IDS_BOOKMARK_TABLE_PATH: {
      std::wstring path;
      BuildPath(node->GetParent(), &path);
      // Force path to have LTR directionality. The whole path needs to be
      // marked with LRE-PDF, so that the path containing both LTR and RTL
      // subfolder names (such as "CBA/FED/(xji)/.x.j.", in which, "CBA" and
      // "FED" are subfolder names in Hebrew) can be displayed as LTR.
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
        l10n_util::WrapStringWithLTRFormatting(&path);
      return path;
    }
  }
  NOTREACHED();
  return std::wstring();
}

SkBitmap BookmarkTableModel::GetIcon(int row) {
  static SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  static SkBitmap* default_icon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_DEFAULT_FAVICON);

  BookmarkNode* node = GetNodeForRow(row);
  if (node->is_folder())
    return *folder_icon;

  if (node->GetFavIcon().empty())
    return *default_icon;

  return node->GetFavIcon();
}

void BookmarkTableModel::BookmarkModelBeingDeleted(BookmarkModel* model) {
  model_->RemoveObserver(this);
  model_ = NULL;
}

int BookmarkTableModel::IndexOfNode(BookmarkNode* node) {
  for (int i = RowCount() - 1; i >= 0; --i) {
    if (GetNodeForRow(i) == node)
      return i;
  }
  return -1;
}

void BookmarkTableModel::BuildPath(BookmarkNode* node, std::wstring* path) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node == model()->GetBookmarkBarNode()) {
    *path = l10n_util::GetString(IDS_BOOKMARK_TABLE_BOOKMARK_BAR_PATH);
    return;
  }
  if (node == model()->other_node()) {
    *path = l10n_util::GetString(IDS_BOOKMARK_TABLE_OTHER_BOOKMARKS_PATH);
    return;
  }
  BuildPath(node->GetParent(), path);
  path->append(l10n_util::GetString(IDS_BOOKMARK_TABLE_PATH_SEPARATOR));
  // Force path to have LTR directionality. Otherwise, folder path "CBA/FED"
  // (in which, "CBA" and "FED" stand for folder names in Hebrew, and "FED" is
  // a subfolder of "CBA") will be displayed as "FED/CBA".
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    path->push_back(static_cast<wchar_t>(l10n_util::kLeftToRightMark));
  }
  path->append(node->GetTitle());
}
