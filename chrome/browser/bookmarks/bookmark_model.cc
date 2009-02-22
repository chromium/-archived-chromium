// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model.h"

#include "base/gfx/png_decoder.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/profile.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/scoped_vector.h"
#include "grit/generated_resources.h"

using base::Time;

// BookmarkNode ---------------------------------------------------------------

namespace {

// ID for BookmarkNodes.
// Various places assume an invalid id if == 0, for that reason we start with 1.
int next_id_ = 1;

}

const SkBitmap& BookmarkNode::GetFavIcon() {
  if (!loaded_favicon_) {
    loaded_favicon_ = true;
    model_->LoadFavIcon(this);
  }
  return favicon_;
}

BookmarkNode::BookmarkNode(BookmarkModel* model, const GURL& url)
    : model_(model),
      id_(next_id_++),
      loaded_favicon_(false),
      favicon_load_handle_(0),
      url_(url),
      type_(!url.is_empty() ? history::StarredEntry::URL :
            history::StarredEntry::BOOKMARK_BAR),
      date_added_(Time::Now()) {
}

void BookmarkNode::Reset(const history::StarredEntry& entry) {
  DCHECK(entry.type != history::StarredEntry::URL ||
         entry.url == url_);

  favicon_ = SkBitmap();
  type_ = entry.type;
  date_added_ = entry.date_added;
  date_group_modified_ = entry.date_group_modified;
  SetTitle(entry.title);
}

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(root_(this, GURL())),
      bookmark_bar_node_(NULL),
      other_node_(NULL),
      observers_(ObserverList<BookmarkModelObserver>::NOTIFY_EXISTING_ONLY),
      waiting_for_history_load_(false)
#if defined(OS_WIN)
    , loaded_signal_(CreateEvent(NULL, TRUE, FALSE, NULL))
#endif
{
  // Create the bookmark bar and other bookmarks folders. These always exist.
  CreateBookmarkNode();
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

BookmarkModel::~BookmarkModel() {
  if (profile_ && store_.get()) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::FAVICON_CHANGED, Source<Profile>(profile_));
  }

  if (waiting_for_history_load_) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::HISTORY_LOADED, Source<Profile>(profile_));
  }

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkModelBeingDeleted(this));

  if (store_) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->BookmarkModelDeleted();
  }
}

void BookmarkModel::Load() {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  LOG(INFO) << "Loading bookmarks";

  // Listen for changes to favicons so that we can update the favicon of the
  // node appropriately.
  NotificationService::current()->AddObserver(
      this,NotificationType::FAVICON_CHANGED, Source<Profile>(profile_));

  // Load the bookmarks. BookmarkStorage notifies us when done.
  store_ = new BookmarkStorage(profile_, this);
  store_->LoadBookmarks(false);
}

BookmarkNode* BookmarkModel::GetParentForNewNodes() {
  std::vector<BookmarkNode*> nodes =
      bookmark_utils::GetMostRecentlyModifiedGroups(this, 1);
  return nodes.empty() ? bookmark_bar_node_ : nodes[0];
}

void BookmarkModel::Remove(BookmarkNode* parent, int index) {
  if (!loaded_ || !IsValidIndex(parent, index, false) || parent == &root_) {
    NOTREACHED();
    return;
  }
  RemoveAndDeleteNode(parent->GetChild(index));
}

void BookmarkModel::Move(BookmarkNode* node,
                         BookmarkNode* new_parent,
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

  BookmarkNode* old_parent = node->GetParent();
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

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeMoved(this, old_parent, old_index,
                                      new_parent, index));
}

void BookmarkModel::SetTitle(BookmarkNode* node,
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

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

void BookmarkModel::GetNodesByURL(const GURL& url,
                                  std::vector<BookmarkNode*>* nodes) {
  AutoLock url_lock(url_lock_);
  BookmarkNode tmp_node(this, url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->GetURL() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

BookmarkNode* BookmarkModel::GetMostRecentlyAddedNodeForURL(const GURL& url) {
  std::vector<BookmarkNode*> nodes;
  GetNodesByURL(url, &nodes);
  if (nodes.empty())
    return NULL;

  std::sort(nodes.begin(), nodes.end(), &bookmark_utils::MoreRecentlyAdded);
  return nodes.front();
}

void BookmarkModel::GetBookmarks(std::vector<GURL>* urls) {
  AutoLock url_lock(url_lock_);
  const GURL* last_url = NULL;
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    const GURL* url = &((*i)->url_);
    // Only add unique URLs.
    if (!last_url || *url != *last_url)
      urls->push_back(*url);
    last_url = url;
  }
}

bool BookmarkModel::IsBookmarked(const GURL& url) {
  AutoLock url_lock(url_lock_);
  return IsBookmarkedNoLock(url);
}

BookmarkNode* BookmarkModel::GetNodeByID(int id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(&root_, id);
}

BookmarkNode* BookmarkModel::AddGroup(
    BookmarkNode* parent,
    int index,
    const std::wstring& title) {
  if (!loaded_ || parent == &root_ || !IsValidIndex(parent, index, true)) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }

  BookmarkNode* new_node = new BookmarkNode(this, GURL());
  new_node->date_group_modified_ = Time::Now();
  new_node->SetTitle(title);
  new_node->type_ = history::StarredEntry::USER_GROUP;

  return AddNode(parent, index, new_node, false);
}

BookmarkNode* BookmarkModel::AddURL(BookmarkNode* parent,
                                    int index,
                                    const std::wstring& title,
                                    const GURL& url) {
  return AddURLWithCreationTime(parent, index, title, url, Time::Now());
}

BookmarkNode* BookmarkModel::AddURLWithCreationTime(
    BookmarkNode* parent,
    int index,
    const std::wstring& title,
    const GURL& url,
    const Time& creation_time) {
  if (!loaded_ || !url.is_valid() || parent == &root_ ||
      !IsValidIndex(parent, index, true)) {
    NOTREACHED();
    return NULL;
  }

  bool was_bookmarked = IsBookmarked(url);

  SetDateGroupModified(parent, creation_time);

  BookmarkNode* new_node = new BookmarkNode(this, url);
  new_node->SetTitle(title);
  new_node->date_added_ = creation_time;
  new_node->type_ = history::StarredEntry::URL;

  {
    // Only hold the lock for the duration of the insert.
    AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node);
  }

  return AddNode(parent, index, new_node, was_bookmarked);
}

void BookmarkModel::SetURLStarred(const GURL& url,
                                  const std::wstring& title,
                                  bool is_starred) {
  std::vector<BookmarkNode*> bookmarks;
  GetNodesByURL(url, &bookmarks);
  bool bookmarks_exist = !bookmarks.empty();
  if (is_starred == bookmarks_exist)
    return;  // Nothing to do, state already matches.

  if (is_starred) {
    // Create a bookmark.
    BookmarkNode* parent = GetParentForNewNodes();
    AddURL(parent, parent->GetChildCount(), title, url);
  } else {
    // Remove all the bookmarks.
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      BookmarkNode* node = bookmarks[i];
      Remove(node->GetParent(), node->GetParent()->IndexOfChild(node));
    }
  }
}

void BookmarkModel::ResetDateGroupModified(BookmarkNode* node) {
  SetDateGroupModified(node, Time());
}

void BookmarkModel::ClearStore() {
  if (profile_ && store_.get()) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::FAVICON_CHANGED, Source<Profile>(profile_));
  }
  store_ = NULL;
}

bool BookmarkModel::IsBookmarkedNoLock(const GURL& url) {
  BookmarkNode tmp_node(this, url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void BookmarkModel::FavIconLoaded(BookmarkNode* node) {
  // Send out notification to the observer.
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeFavIconLoaded(this, node));
}

void BookmarkModel::RemoveNode(BookmarkNode* node,
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
    // i points to the first node with the URL, advance until we find the
    // node we're removing.
    while (*i != node)
      ++i;
    nodes_ordered_by_url_set_.erase(i);
    removed_urls->insert(node->GetURL());
  }

  CancelPendingFavIconLoadRequests(node);

  // Recurse through children.
  for (int i = node->GetChildCount() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i), removed_urls);
}

void BookmarkModel::OnBookmarkStorageLoadedBookmarks(
    bool file_exists,
    bool loaded_from_history) {
  if (loaded_) {
    NOTREACHED();
    return;
  }

  LOG(INFO) << "Loaded bookmarks, file_exists=" << file_exists <<
      " from_history=" << loaded_from_history;

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
    LOG(INFO) << " waiting for history to finish";

    waiting_for_history_load_ = true;
    NotificationService::current()->AddObserver(
        this, NotificationType::HISTORY_LOADED, Source<Profile>(profile_));
  } else {
    OnHistoryDone();
  }
}

void BookmarkModel::OnHistoryDone() {
  if (loaded_) {
    NOTREACHED();
    return;
  }

  LOG(INFO) << " history done, reloading";

  // If the bookmarks were stored in the db the db will have migrated them to
  // a file now. Try loading from the file.
  store_->LoadBookmarks(true);
}

void BookmarkModel::DoneLoading() {
  {
    AutoLock url_lock(url_lock_);
    // Update nodes_ordered_by_url_set_ from the nodes.
    PopulateNodesByURL(&root_);
  }

  loaded_ = true;

#if defined(OS_WIN)
  if (loaded_signal_.Get())
    SetEvent(loaded_signal_.Get());
#else
  NOTIMPLEMENTED();
#endif


  // Notify our direct observers.
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_, Loaded(this));

  // And generic notification.
  NotificationService::current()->Notify(
      NotificationType::BOOKMARK_MODEL_LOADED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

void BookmarkModel::RemoveAndDeleteNode(BookmarkNode* delete_me) {
  scoped_ptr<BookmarkNode> node(delete_me);

  BookmarkNode* parent = node->GetParent();
  DCHECK(parent);
  int index = parent->IndexOfChild(node.get());
  parent->Remove(index);
  history::URLsStarredDetails details(false);
  {
    AutoLock url_lock(url_lock_);
    RemoveNode(node.get(), &details.changed_urls);

    // RemoveNode adds an entry to changed_urls for each node of type URL. As we
    // allow duplicates we need to remove any entries that are still bookmarked.
    for (std::set<GURL>::iterator i = details.changed_urls.begin();
         i != details.changed_urls.end(); ){
      if (IsBookmarkedNoLock(*i)) {
        // When we erase the iterator pointing at the erasee is
        // invalidated, so using i++ here within the "erase" call is
        // important as it advances the iterator before passing the
        // old value through to erase.
        details.changed_urls.erase(i++);
      } else {
        ++i;
      }
    }
  }

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeRemoved(this, parent, index, node.get()));

  if (details.changed_urls.empty()) {
    // No point in sending out notification if the starred state didn't change.
    return;
  }

  if (profile_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->URLsNoLongerBookmarked(details.changed_urls);
  }

  NotificationService::current()->Notify(
      NotificationType::URLS_STARRED,
      Source<Profile>(profile_),
      Details<history::URLsStarredDetails>(&details));
}

BookmarkNode* BookmarkModel::AddNode(BookmarkNode* parent,
                                     int index,
                                     BookmarkNode* node,
                                     bool was_bookmarked) {
  parent->Add(index, node);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeAdded(this, parent, index));

  if (node->GetType() == history::StarredEntry::URL && !was_bookmarked) {
    history::URLsStarredDetails details(true);
    details.changed_urls.insert(node->GetURL());
    NotificationService::current()->Notify(
        NotificationType::URLS_STARRED,
        Source<Profile>(profile_),
        Details<history::URLsStarredDetails>(&details));
  }
  return node;
}

void BookmarkModel::BlockTillLoaded() {
#if defined(OS_WIN)
  if (loaded_signal_.Get())
    WaitForSingleObject(loaded_signal_.Get(), INFINITE);
#else
  NOTIMPLEMENTED();
#endif
}

BookmarkNode* BookmarkModel::GetNodeByID(BookmarkNode* node, int id) {
  if (node->id() == id)
    return node;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkNode* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool BookmarkModel::IsValidIndex(BookmarkNode* parent,
                                 int index,
                                 bool allow_end) {
  return (parent && parent->is_folder() &&
          (index >= 0 && (index < parent->GetChildCount() ||
                          (allow_end && index == parent->GetChildCount()))));
  }

void BookmarkModel::SetDateGroupModified(BookmarkNode* parent,
                                         const Time time) {
  DCHECK(parent);
  parent->date_group_modified_ = time;

  if (store_.get())
    store_->ScheduleSave();
}

void BookmarkModel::CreateBookmarkNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  bookmark_bar_node_ = CreateRootNodeFromStarredEntry(entry);
}

void BookmarkModel::CreateOtherBookmarksNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::OTHER;
  other_node_ = CreateRootNodeFromStarredEntry(entry);
}

BookmarkNode* BookmarkModel::CreateRootNodeFromStarredEntry(
    const history::StarredEntry& entry) {
  DCHECK(entry.type == history::StarredEntry::BOOKMARK_BAR ||
         entry.type == history::StarredEntry::OTHER);
  BookmarkNode* node = new BookmarkNode(this, GURL());
  node->Reset(entry);
  if (entry.type == history::StarredEntry::BOOKMARK_BAR)
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  else
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));
  return node;
}

void BookmarkModel::OnFavIconDataAvailable(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  SkBitmap fav_icon;
  BookmarkNode* node =
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

void BookmarkModel::LoadFavIcon(BookmarkNode* node) {
  if (node->GetType() != history::StarredEntry::URL)
    return;

  DCHECK(node->GetURL().is_valid());
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  HistoryService::Handle handle = history_service->GetFavIconForURL(
      node->GetURL(), &load_consumer_,
      NewCallback(this, &BookmarkModel::OnFavIconDataAvailable));
  load_consumer_.SetClientData(history_service, handle, node);
  node->favicon_load_handle_ = handle;
}

void BookmarkModel::CancelPendingFavIconLoadRequests(BookmarkNode* node) {
  if (node->favicon_load_handle_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->CancelRequest(node->favicon_load_handle_);
    node->favicon_load_handle_ = 0;
  }
}

void BookmarkModel::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::FAVICON_CHANGED: {
      // Prevent the observers from getting confused for multiple favicon loads.
      Details<history::FavIconChangeDetails> favicon_details(details);
      for (std::set<GURL>::const_iterator i = favicon_details->urls.begin();
           i != favicon_details->urls.end(); ++i) {
        std::vector<BookmarkNode*> nodes;
        GetNodesByURL(*i, &nodes);
        for (size_t i = 0; i < nodes.size(); ++i) {
          // Got an updated favicon, for a URL, do a new request.
          BookmarkNode* node = nodes[i];
          node->InvalidateFavicon();
          CancelPendingFavIconLoadRequests(node);
          FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                            BookmarkNodeChanged(this, node));
        }
      }
      break;
    }

    case NotificationType::HISTORY_LOADED: {
      if (waiting_for_history_load_) {
        waiting_for_history_load_ = false;
        NotificationService::current()->RemoveObserver(
            this,NotificationType::HISTORY_LOADED, Source<Profile>(profile_));
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

void BookmarkModel::PopulateNodesByURL(BookmarkNode* node) {
  // NOTE: this is called with url_lock_ already held. As such, this doesn't
  // explicitly grab the lock.
  if (node->is_url())
    nodes_ordered_by_url_set_.insert(node);
  for (int i = 0; i < node->GetChildCount(); ++i)
    PopulateNodesByURL(node->GetChild(i));
}
