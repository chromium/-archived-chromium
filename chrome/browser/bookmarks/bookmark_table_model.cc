// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_table_model.h"

#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/table_model_observer.h"
#include "base/string_util.h"
#include "base/time_format.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

namespace {

// Number of bookmarks shown in recently bookmarked.
const int kRecentlyBookmarkedCount = 50;

// VectorBackedBookmarkTableModel ----------------------------------------------

class VectorBackedBookmarkTableModel : public BookmarkTableModel {
 public:
  explicit VectorBackedBookmarkTableModel(BookmarkModel* model)
      : BookmarkTableModel(model) {
  }

  virtual const BookmarkNode* GetNodeForRow(int row) {
    return nodes_[row];
  }

  virtual int RowCount() {
    return static_cast<int>(nodes_.size());
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {
    NotifyObserverOfChange(new_parent->GetChild(new_index));
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
    NotifyObserverOfChange(node);
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {
    NotifyObserverOfChange(node);
  }

 protected:
  void NotifyObserverOfChange(const BookmarkNode* node) {
    if (!observer())
      return;

    int index = IndexOfNode(node);
    if (index != -1)
      observer()->OnItemsChanged(index, 1);
  }

  typedef std::vector<const BookmarkNode*> Nodes;
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
  FolderBookmarkTableModel(BookmarkModel* model, const BookmarkNode* root_node)
      : VectorBackedBookmarkTableModel(model),
        root_node_(root_node) {
    PopulateNodesFromRoot();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
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
                                 const BookmarkNode* parent,
                                 int index) {
    if (root_node_ == parent) {
      nodes().insert(nodes().begin() + index, parent->GetChild(index));
      if (observer())
        observer()->OnItemsAdded(index, 1);
    }
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node) {
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
                                   const BookmarkNode* node) {
    NotifyChanged(node);
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
    NotifyChanged(node);
  }

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {
    if (node != root_node_)
      return;

    nodes().clear();
    PopulateNodesFromRoot();

    if (observer())
      observer()->OnModelChanged();
  }

 private:
  void NotifyChanged(const BookmarkNode* node) {
    if (node->GetParent() == root_node_ && observer())
      observer()->OnItemsChanged(node->GetParent()->IndexOfChild(node), 1);
  }

  void PopulateNodesFromRoot() {
    for (int i = 0; i < root_node_->GetChildCount(); ++i)
      nodes().push_back(root_node_->GetChild(i));
  }

  // The node we're showing the children of. This is set to NULL if the node
  // (or one of its ancestors) is removed from the model.
  const BookmarkNode* root_node_;

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
                                 const BookmarkNode* parent,
                                 int index) {
    if (parent->GetChild(index)->is_url())
      UpdateRecentlyBookmarked();
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* old_node) {
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
                           const std::wstring& search_text,
                           const std::wstring& languages)
      : VectorBackedBookmarkTableModel(model),
        search_text_(search_text),
        languages_(languages) {
    bookmark_utils::GetBookmarksContainingText(
        model, search_text, std::numeric_limits<int>::max(),
        languages, &nodes());
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {
    const BookmarkNode* node = parent->GetChild(index);
    if (bookmark_utils::DoesBookmarkContainText(
        node, search_text_, languages_)) {
      nodes().push_back(node);
      if (observer())
        observer()->OnItemsAdded(static_cast<int>(nodes().size() - 1), 1);
    }
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node) {
    int internal_index = IndexOfNode(node);
    if (internal_index == -1)
      return;

    nodes().erase(nodes().begin() + static_cast<int>(internal_index));
    if (observer())
      observer()->OnItemsRemoved(internal_index, 1);
  }

 private:
  const std::wstring search_text_;
  const std::wstring languages_;

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
    BookmarkModel* model, const BookmarkNode* node) {
  return new FolderBookmarkTableModel(model, node);
}

// static
BookmarkTableModel* BookmarkTableModel::CreateSearchTableModel(
    BookmarkModel* model,
    const std::wstring& text,
    const std::wstring& languages) {
  return new BookmarkSearchTableModel(model, text, languages);
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
  const BookmarkNode* node = GetNodeForRow(row);
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
      std::wstring languages = model_ && model_->profile()
          ? model_->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages)
          : std::wstring();
      std::wstring url_text =
          net::FormatUrl(node->GetURL(), languages, false, UnescapeRule::SPACES,
          NULL, NULL);
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
        l10n_util::WrapStringWithLTRFormatting(&url_text);
      return url_text;
    }

    case IDS_BOOKMARK_TABLE_PATH: {
      std::wstring path;
      BuildPath(node->GetParent(), &path);
      // Force path to have LTR directionality. The whole path (but not every
      // single path component) is marked with LRE-PDF. For example,
      // ALEPH/BET/GIMEL (using uppercase English for Hebrew) is supposed to
      // appear (visually) as LEMIG/TEB/HPELA; foo/C/B/A.doc refers to file
      // C.doc in directory B in directory A in directory foo, not to file
      // A.doc in directory B in directory C in directory foo. The reason to
      // mark the whole path, but not every single path component, as LTR is
      // because paths need to get written in text documents, and that is how
      // they will appear there. Being a saint and doing the tedious formatting
      // to every single path component to get it to come out in the logical
      // order will accomplish nothing but confuse people, since they will now
      // see both formats being used, and will never know what anything means.
      // Furthermore, doing the "logical" formatting with characters like LRM,
      // LRE, and PDF to every single path component means that when someone
      // copy/pastes your path, it will still contain those characters, and
      // trying to access the file will fail because of them. Windows Explorer,
      // Firefox, IE, Nautilus, gedit choose to format only the whole path as
      // LTR too. The point here is to display the path the same way as it's
      // displayed by other software.
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

  const BookmarkNode* node = GetNodeForRow(row);
  if (node->is_folder())
    return *folder_icon;

  if (model_->GetFavIcon(node).empty())
    return *default_icon;

  return model_->GetFavIcon(node);
}

void BookmarkTableModel::BookmarkModelBeingDeleted(BookmarkModel* model) {
  model_->RemoveObserver(this);
  model_ = NULL;
}

int BookmarkTableModel::IndexOfNode(const BookmarkNode* node) {
  for (int i = RowCount() - 1; i >= 0; --i) {
    if (GetNodeForRow(i) == node)
      return i;
  }
  return -1;
}

void BookmarkTableModel::BuildPath(const BookmarkNode* node,
                                   std::wstring* path) {
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
  path->append(node->GetTitle());
}
