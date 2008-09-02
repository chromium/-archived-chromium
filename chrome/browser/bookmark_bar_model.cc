// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmark_bar_model.h"

#include "base/gfx/png_decoder.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/bookmark_storage.h"
#include "chrome/common/scoped_vector.h"

#include "generated_resources.h"

namespace {

// Functions used for sorting.
bool MoreRecentlyModified(BookmarkBarNode* n1, BookmarkBarNode* n2) {
  return n1->date_group_modified() > n2->date_group_modified();
}

bool MoreRecentlyAdded(BookmarkBarNode* n1, BookmarkBarNode* n2) {
  return n1->date_added() > n2->date_added();
}

}  // namespace

// BookmarkBarNode ------------------------------------------------------------

namespace {

// ID for BookmarkBarNodes.
// Various places assume an invalid id if == 0, for that reason we start with 1.
int next_id_ = 1;

}

const SkBitmap& BookmarkBarNode::GetFavIcon() {
  if (!loaded_favicon_) {
    loaded_favicon_ = true;
    model_->LoadFavIcon(this);
  }
  return favicon_;
}

BookmarkBarNode::BookmarkBarNode(BookmarkBarModel* model, const GURL& url)
    : model_(model),
      id_(next_id_++),
      loaded_favicon_(false),
      favicon_load_handle_(0),
      url_(url),
      type_(!url.is_empty() ? history::StarredEntry::URL :
            history::StarredEntry::BOOKMARK_BAR),
      date_added_(Time::Now()) {
}

void BookmarkBarNode::Reset(const history::StarredEntry& entry) {
  DCHECK(entry.type != history::StarredEntry::URL ||
         entry.url == url_);

  favicon_ = SkBitmap();
  type_ = entry.type;
  date_added_ = entry.date_added;
  date_group_modified_ = entry.date_group_modified;
  SetTitle(entry.title);
}

// BookmarkBarModel -----------------------------------------------------------

BookmarkBarModel::BookmarkBarModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      root_(this, GURL()),
      bookmark_bar_node_(NULL),
      other_node_(NULL),
      waiting_for_history_load_(false),
      loaded_signal_(CreateEvent(NULL, TRUE, FALSE, NULL)) {
  // Create the bookmark bar and other bookmarks folders. These always exist.
  CreateBookmarkBarNode();
  CreateOtherBookmarksNode();

  // And add them to the root.
  //
  // WARNING: order is important here, various places assume bookmark bar then
  // other node.
  root_.Add(0, bookmark_bar_node_);
  root_.Add(1, other_node_);

  if (!profile_) {
    // Profile is null during testing.
    DoneLoading();
  }
}

BookmarkBarModel::~BookmarkBarModel() {
  if (profile_ && store_.get()) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_FAVICON_CHANGED, Source<Profile>(profile_));
  }

  if (waiting_for_history_load_) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_HISTORY_LOADED, Source<Profile>(profile_));
  }

  if (store_) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->BookmarkModelDeleted();
  }
}

void BookmarkBarModel::Load() {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  // Listen for changes to favicons so that we can update the favicon of the
  // node appropriately.
  NotificationService::current()->AddObserver(
      this, NOTIFY_FAVICON_CHANGED, Source<Profile>(profile_));

  // Load the bookmarks. BookmarkStorage notifies us when done.
  store_ = new BookmarkStorage(profile_, this);
  store_->LoadBookmarks(false);
}

BookmarkBarNode* BookmarkBarModel::GetParentForNewNodes() {
  std::vector<BookmarkBarNode*> nodes;

  GetMostRecentlyModifiedGroupNodes(&root_, 1, &nodes);
  return nodes.empty() ? bookmark_bar_node_ : nodes[0];
}

std::vector<BookmarkBarNode*> BookmarkBarModel::GetMostRecentlyModifiedGroups(
    size_t max_count) {
  std::vector<BookmarkBarNode*> nodes;
  GetMostRecentlyModifiedGroupNodes(&root_, max_count, &nodes);

  if (nodes.size() < max_count) {
    // Add the bookmark bar and other nodes if there is space.
    if (find(nodes.begin(), nodes.end(), bookmark_bar_node_) == nodes.end())
      nodes.push_back(bookmark_bar_node_);

    if (nodes.size() < max_count &&
        find(nodes.begin(), nodes.end(), other_node_) == nodes.end()) {
      nodes.push_back(other_node_);
    }
  }
  return nodes;
}

void BookmarkBarModel::GetMostRecentlyAddedEntries(
    size_t count,
    std::vector<BookmarkBarNode*>* nodes) {
  AutoLock url_lock(url_lock_);
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    std::vector<BookmarkBarNode*>::iterator insert_position =
        std::upper_bound(nodes->begin(), nodes->end(), *i, &MoreRecentlyAdded);
    if (nodes->size() < count || insert_position != nodes->end()) {
      nodes->insert(insert_position, *i);
      while (nodes->size() > count)
        nodes->pop_back();
    }
  }
}

void BookmarkBarModel::GetBookmarksMatchingText(
    const std::wstring& text,
    size_t max_count,
    std::vector<TitleMatch>* matches) {
  QueryParser parser;
  ScopedVector<QueryNode> query_nodes;
  parser.ParseQuery(text, &query_nodes.get());
  if (query_nodes.empty())
    return;

  AutoLock url_lock(url_lock_);
  Snippet::MatchPositions match_position;
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    if (parser.DoesQueryMatch((*i)->GetTitle(), query_nodes.get(),
                              &match_position)) {
      matches->push_back(TitleMatch());
      matches->back().node = *i;
      matches->back().match_positions.swap(match_position);
      if (matches->size() == max_count)
        break;
    }
  }
}

void BookmarkBarModel::Remove(BookmarkBarNode* parent, int index) {
  if (!loaded_ || !IsValidIndex(parent, index, false) || parent == &root_) {
    NOTREACHED();
    return;
  }
  RemoveAndDeleteNode(parent->GetChild(index));
}

void BookmarkBarModel::Move(BookmarkBarNode* node,
                            BookmarkBarNode* new_parent,
                            int index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index, true) ||
      new_parent == &root_ || node == &root_ || node == bookmark_bar_node_ ||
      node == other_node_) {
    NOTREACHED();
    return;
  }

  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return;
  }

  SetDateGroupModified(new_parent, Time::Now());

  BookmarkBarNode* old_parent = node->GetParent();
  int old_index = old_parent->IndexOfChild(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  if (old_parent == new_parent && index > old_index)
    index--;
  new_parent->Add(index, node);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeMoved(this, old_parent, old_index,
                                      new_parent, index));
}

void BookmarkBarModel::SetTitle(BookmarkBarNode* node,
                                const std::wstring& title) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node->GetTitle() == title)
    return;

  node->SetTitle(title);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

BookmarkBarNode* BookmarkBarModel::GetNodeByURL(const GURL& url) {
  AutoLock url_lock(url_lock_);
  BookmarkBarNode tmp_node(this, url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  return (i != nodes_ordered_by_url_set_.end()) ? *i : NULL;
}

void BookmarkBarModel::GetBookmarks(std::vector<GURL>* urls) {
  AutoLock url_lock(url_lock_);
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    urls->push_back((*i)->GetURL());
  }
}

BookmarkBarNode* BookmarkBarModel::GetNodeByID(int id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(&root_, id);
}

BookmarkBarNode* BookmarkBarModel::AddGroup(
    BookmarkBarNode* parent,
    int index,
    const std::wstring& title) {
  if (!loaded_ || parent == &root_ || !IsValidIndex(parent, index, true)) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }

  BookmarkBarNode* new_node = new BookmarkBarNode(this, GURL());
  new_node->SetTitle(title);
  new_node->type_ = history::StarredEntry::USER_GROUP;

  return AddNode(parent, index, new_node);
}

BookmarkBarNode* BookmarkBarModel::AddURL(BookmarkBarNode* parent,
                                          int index,
                                          const std::wstring& title,
                                          const GURL& url) {
  return AddURLWithCreationTime(parent, index, title, url, Time::Now());
}

BookmarkBarNode* BookmarkBarModel::AddURLWithCreationTime(
    BookmarkBarNode* parent,
    int index,
    const std::wstring& title,
    const GURL& url,
    const Time& creation_time) {
  if (!loaded_ || !url.is_valid() || parent == &root_ ||
      !IsValidIndex(parent, index, true)) {
    NOTREACHED();
    return NULL;
  }

  BookmarkBarNode* existing_node = GetNodeByURL(url);
  if (existing_node) {
    Move(existing_node, parent, index);
    SetTitle(existing_node, title);
    return existing_node;
  }

  SetDateGroupModified(parent, creation_time);

  BookmarkBarNode* new_node = new BookmarkBarNode(this, url);
  new_node->SetTitle(title);
  new_node->date_added_ = creation_time;
  new_node->type_ = history::StarredEntry::URL;

  AutoLock url_lock(url_lock_);
  nodes_ordered_by_url_set_.insert(new_node);

  return AddNode(parent, index, new_node);
}

void BookmarkBarModel::SetURLStarred(const GURL& url,
                                     const std::wstring& title,
                                     bool is_starred) {
  BookmarkBarNode* node = GetNodeByURL(url);
  if (is_starred && !node) {
    // Add the url.
    BookmarkBarNode* parent = GetParentForNewNodes();
    AddURL(parent, parent->GetChildCount(), title, url);
  } else if (!is_starred && node) {
    Remove(node->GetParent(), node->GetParent()->IndexOfChild(node));
  }
}

void BookmarkBarModel::ResetDateGroupModified(BookmarkBarNode* node) {
  SetDateGroupModified(node, Time());
}

void BookmarkBarModel::FavIconLoaded(BookmarkBarNode* node) {
  // Send out notification to the observer.
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeFavIconLoaded(this, node));
}

void BookmarkBarModel::RemoveNode(BookmarkBarNode* node,
                                  std::set<GURL>* removed_urls) {
  if (!loaded_ || !node || node == &root_ || node == bookmark_bar_node_ ||
      node == other_node_) {
    NOTREACHED();
    return;
  }

  if (node->GetType() == history::StarredEntry::URL) {
    // NOTE: this is called in such a way that url_lock_ is already held. As
    // such, this doesn't explicitly grab the lock.
    NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(node);
    DCHECK(i != nodes_ordered_by_url_set_.end());
    nodes_ordered_by_url_set_.erase(i);
    removed_urls->insert(node->GetURL());
  }

  CancelPendingFavIconLoadRequests(node);

  // Recurse through children.
  for (int i = node->GetChildCount() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i), removed_urls);
}

void BookmarkBarModel::OnBookmarkStorageLoadedBookmarks(
    bool file_exists,
    bool loaded_from_history) {
  if (loaded_) {
    NOTREACHED();
    return;
  }

  if (file_exists || loaded_from_history || !profile_ ||
      !profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)) {
    // The file exists, we're loaded.
    DoneLoading();

    if (loaded_from_history) {
      // We were just populated from the historical file. Schedule a save so
      // that the main file is up to date.
      store_->ScheduleSave();
    }
    return;
  }

  // The file doesn't exist. This means one of two things:
  // 1. A clean profile.
  // 2. The user is migrating from an older version where bookmarks were saved
  //    in history.
  // We assume step 2. If history had the bookmarks, history will write the
  // bookmarks to a file for us. We need to wait until history has finished
  // loading before reading from that file.
  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history->backend_loaded()) {
    // The backend isn't finished loading. Wait for it.
    waiting_for_history_load_ = true;
    NotificationService::current()->AddObserver(
        this, NOTIFY_HISTORY_LOADED, Source<Profile>(profile_));
  } else {
    OnHistoryDone();
  }
}

void BookmarkBarModel::OnHistoryDone() {
  if (loaded_) {
    NOTREACHED();
    return;
  }

  // If the bookmarks were stored in the db the db will have migrated them to
  // a file now. Try loading from the file.
  store_->LoadBookmarks(true);
}

void BookmarkBarModel::DoneLoading() {
  {
    AutoLock url_lock(url_lock_);
    // Update nodes_ordered_by_url_set_ from the nodes.
    PopulateNodesByURL(&root_);
  }

  loaded_ = true;

  if (loaded_signal_.Get())
    SetEvent(loaded_signal_.Get());


  // Notify our direct observers.
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_, Loaded(this));

  // And generic notification.
  NotificationService::current()->Notify(
      NOTIFY_BOOKMARK_MODEL_LOADED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

void BookmarkBarModel::RemoveAndDeleteNode(BookmarkBarNode* delete_me) {
  scoped_ptr<BookmarkBarNode> node(delete_me);

  BookmarkBarNode* parent = node->GetParent();
  DCHECK(parent);
  int index = parent->IndexOfChild(node.get());
  parent->Remove(index);
  history::URLsStarredDetails details(false);
  {
    AutoLock url_lock(url_lock_);
    RemoveNode(node.get(), &details.changed_urls);
  }

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeRemoved(this, parent, index));

  if (profile_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->URLsNoLongerBookmarked(details.changed_urls);
  }

  NotificationService::current()->Notify(NOTIFY_URLS_STARRED,
      Source<Profile>(profile_),
      Details<history::URLsStarredDetails>(&details));
}

BookmarkBarNode* BookmarkBarModel::AddNode(BookmarkBarNode* parent,
                                           int index,
                                           BookmarkBarNode* node) {
  parent->Add(index, node);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeAdded(this, parent, index));

  if (node->GetType() == history::StarredEntry::URL) {
    history::URLsStarredDetails details(true);
    details.changed_urls.insert(node->GetURL());
    NotificationService::current()->Notify(NOTIFY_URLS_STARRED,
        Source<Profile>(profile_),
        Details<history::URLsStarredDetails>(&details));
  }
  return node;
}

void BookmarkBarModel::BlockTillLoaded() {
  if (loaded_signal_.Get())
    WaitForSingleObject(loaded_signal_.Get(), INFINITE);
}

BookmarkBarNode* BookmarkBarModel::GetNodeByID(BookmarkBarNode* node,
                                               int id) {
  if (node->id() == id)
    return node;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkBarNode* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool BookmarkBarModel::IsValidIndex(BookmarkBarNode* parent,
                                    int index,
                                    bool allow_end) {
  return (parent &&
          (index >= 0 && (index < parent->GetChildCount() ||
                          (allow_end && index == parent->GetChildCount()))));
  }

void BookmarkBarModel::SetDateGroupModified(BookmarkBarNode* parent,
                                            const Time time) {
  DCHECK(parent);
  parent->date_group_modified_ = time;

  if (store_.get())
    store_->ScheduleSave();
}

void BookmarkBarModel::CreateBookmarkBarNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  bookmark_bar_node_ = CreateRootNodeFromStarredEntry(entry);
}

void BookmarkBarModel::CreateOtherBookmarksNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::OTHER;
  other_node_ = CreateRootNodeFromStarredEntry(entry);
}

BookmarkBarNode* BookmarkBarModel::CreateRootNodeFromStarredEntry(
    const history::StarredEntry& entry) {
  DCHECK(entry.type == history::StarredEntry::BOOKMARK_BAR ||
         entry.type == history::StarredEntry::OTHER);
  BookmarkBarNode* node = new BookmarkBarNode(this, GURL());
  node->Reset(entry);
  if (entry.type == history::StarredEntry::BOOKMARK_BAR)
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  else
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));
  return node;
}

void BookmarkBarModel::OnFavIconDataAvailable(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  SkBitmap fav_icon;
  BookmarkBarNode* node =
      load_consumer_.GetClientData(
          profile_->GetHistoryService(Profile::EXPLICIT_ACCESS), handle);
  DCHECK(node);
  node->favicon_load_handle_ = 0;
  if (know_favicon && data.get() &&
      PNGDecoder::Decode(&data->data, &fav_icon)) {
    node->favicon_ = fav_icon;
    FavIconLoaded(node);
  }
}

void BookmarkBarModel::LoadFavIcon(BookmarkBarNode* node) {
  if (node->GetType() != history::StarredEntry::URL)
    return;

  DCHECK(node->GetURL().is_valid());
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  HistoryService::Handle handle = history_service->GetFavIconForURL(
      node->GetURL(), &load_consumer_,
      NewCallback(this, &BookmarkBarModel::OnFavIconDataAvailable));
  load_consumer_.SetClientData(history_service, handle, node);
  node->favicon_load_handle_ = handle;
}

void BookmarkBarModel::CancelPendingFavIconLoadRequests(BookmarkBarNode* node) {
  if (node->favicon_load_handle_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->CancelRequest(node->favicon_load_handle_);
    node->favicon_load_handle_ = 0;
  }
}

void BookmarkBarModel::GetMostRecentlyModifiedGroupNodes(
    BookmarkBarNode* parent,
    size_t count,
    std::vector<BookmarkBarNode*>* nodes) {
  if (parent != &root_ && parent->is_folder() &&
      parent->date_group_modified() > Time()) {
    if (count == 0) {
      nodes->push_back(parent);
    } else {
      std::vector<BookmarkBarNode*>::iterator i =
          std::upper_bound(nodes->begin(), nodes->end(), parent,
                           &MoreRecentlyModified);
      if (nodes->size() < count || i != nodes->end()) {
        nodes->insert(i, parent);
        while (nodes->size() > count)
          nodes->pop_back();
      }
    }
  }  // else case, the root node, which we don't care about or imported nodes
     // (which have a time of 0).
  for (int i = 0; i < parent->GetChildCount(); ++i) {
    BookmarkBarNode* child = parent->GetChild(i);
    if (child->is_folder())
      GetMostRecentlyModifiedGroupNodes(child, count, nodes);
  }
}

void BookmarkBarModel::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_FAVICON_CHANGED: {
      // Prevent the observers from getting confused for multiple favicon loads.
      Details<history::FavIconChangeDetails> favicon_details(details);
      for (std::set<GURL>::const_iterator i = favicon_details->urls.begin();
           i != favicon_details->urls.end(); ++i) {
        BookmarkBarNode* node = GetNodeByURL(*i);
        if (node) {
          // Got an updated favicon, for a URL, do a new request.
          node->InvalidateFavicon();
          CancelPendingFavIconLoadRequests(node);
          FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                            BookmarkNodeChanged(this, node));
        }
      }
      break;
    }

    case NOTIFY_HISTORY_LOADED: {
      if (waiting_for_history_load_) {
        waiting_for_history_load_ = false;
        NotificationService::current()->RemoveObserver(
            this, NOTIFY_HISTORY_LOADED, Source<Profile>(profile_));
        OnHistoryDone();
      } else {
        NOTREACHED();
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void BookmarkBarModel::PopulateNodesByURL(BookmarkBarNode* node) {
  // NOTE: this is called with url_lock_ already held. As such, this doesn't
  // explicitly grab the lock.
  if (node->is_url())
    nodes_ordered_by_url_set_.insert(node);
  for (int i = 0; i < node->GetChildCount(); ++i)
    PopulateNodesByURL(node->GetChild(i));
}
