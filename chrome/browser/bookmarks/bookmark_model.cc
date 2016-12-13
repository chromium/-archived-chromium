// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model.h"

#include "app/l10n_util.h"
#include "base/gfx/png_decoder.h"
#include "base/scoped_vector.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_index.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

using base::Time;

namespace {

// Helper to get a mutable bookmark node.
static BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // anonymous namespace

// BookmarkNode ---------------------------------------------------------------

BookmarkNode::BookmarkNode(const GURL& url)
    : url_(url) {
  Initialize(0);
}

BookmarkNode::BookmarkNode(int id, const GURL& url)
    : url_(url){
  Initialize(id);
}

void BookmarkNode::Initialize(int id) {
  id_ = id;
  loaded_favicon_ = false;
  favicon_load_handle_ = 0;
  type_ = !url_.is_empty() ? URL : BOOKMARK_BAR;
  date_added_ = Time::Now();
}

void BookmarkNode::Reset(const history::StarredEntry& entry) {
  DCHECK(entry.type != history::StarredEntry::URL || entry.url == url_);

  favicon_ = SkBitmap();
  switch (entry.type) {
    case history::StarredEntry::URL:
      type_ = BookmarkNode::URL;
      break;
    case history::StarredEntry::USER_GROUP:
      type_ = BookmarkNode::FOLDER;
      break;
    case history::StarredEntry::BOOKMARK_BAR:
      type_ = BookmarkNode::BOOKMARK_BAR;
      break;
    case history::StarredEntry::OTHER:
      type_ = BookmarkNode::OTHER_NODE;
      break;
    default:
      NOTREACHED();
  }
  date_added_ = entry.date_added;
  date_group_modified_ = entry.date_group_modified;
  SetTitle(entry.title);
}

// BookmarkModel --------------------------------------------------------------

namespace {

// Constant for persist IDs prefernece.
const wchar_t kPrefPersistIDs[] = L"bookmarks.persist_ids";

// Comparator used when sorting bookmarks. Folders are sorted first, then
  // bookmarks.
class SortComparator : public std::binary_function<const BookmarkNode*,
                                                   const BookmarkNode*,
                                                   bool> {
 public:
  explicit SortComparator(Collator* collator) : collator_(collator) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (const BookmarkNode* n1, const BookmarkNode* n2) {
    if (n1->GetType() == n2->GetType()) {
      // Types are the same, compare the names.
      if (!collator_)
        return n1->GetTitle() < n2->GetTitle();
      return l10n_util::CompareStringWithCollator(collator_, n1->GetTitle(),
                                                  n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  Collator* collator_;
};

}  // namespace

BookmarkModel::BookmarkModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
      persist_ids_(false),
      file_changed_(false),
      root_(GURL()),
      bookmark_bar_node_(NULL),
      other_node_(NULL),
      next_node_id_(1),
      observers_(ObserverList<BookmarkModelObserver>::NOTIFY_EXISTING_ONLY),
      loaded_signal_(TRUE, FALSE) {
  if (!profile_) {
    // Profile is null during testing.
    DoneLoading(CreateLoadDetails());
  }
  RegisterPreferences();
  LoadPreferences();
}

BookmarkModel::~BookmarkModel() {
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
  registrar_.Add(this, NotificationType::FAVICON_CHANGED,
                 Source<Profile>(profile_));

  // Load the bookmarks. BookmarkStorage notifies us when done.
  store_ = new BookmarkStorage(profile_, this);
  store_->LoadBookmarks(CreateLoadDetails());
}

const BookmarkNode* BookmarkModel::GetParentForNewNodes() {
  std::vector<const BookmarkNode*> nodes =
      bookmark_utils::GetMostRecentlyModifiedGroups(this, 1);
  return nodes.empty() ? bookmark_bar_node_ : nodes[0];
}

void BookmarkModel::Remove(const BookmarkNode* parent, int index) {
  if (!loaded_ || !IsValidIndex(parent, index, false) || is_root(parent)) {
    NOTREACHED();
    return;
  }
  RemoveAndDeleteNode(AsMutable(parent->GetChild(index)));
}

void BookmarkModel::Move(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         int index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index, true) ||
      is_root(new_parent) || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return;
  }

  SetDateGroupModified(new_parent, Time::Now());

  const BookmarkNode* old_parent = node->GetParent();
  int old_index = old_parent->IndexOfChild(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  if (old_parent == new_parent && index > old_index)
    index--;
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(index, AsMutable(node));

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeMoved(this, old_parent, old_index,
                                      new_parent, index));
}

const SkBitmap& BookmarkModel::GetFavIcon(const BookmarkNode* node) {
  DCHECK(node);
  if (!node->is_favicon_loaded()) {
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->set_favicon_loaded(true);
    LoadFavIcon(mutable_node);
  }
  return node->favicon();
}

void BookmarkModel::SetTitle(const BookmarkNode* node,
                             const std::wstring& title) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node->GetTitle() == title)
    return;

  // The title index doesn't support changing the title, instead we remove then
  // add it back.
  index_->Remove(node);
  AsMutable(node)->SetTitle(title);
  index_->Add(node);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

void BookmarkModel::GetNodesByURL(const GURL& url,
                                  std::vector<const BookmarkNode*>* nodes) {
  AutoLock url_lock(url_lock_);
  BookmarkNode tmp_node(url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->GetURL() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedNodeForURL(
    const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
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
    const GURL* url = &((*i)->GetURL());
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

const BookmarkNode* BookmarkModel::GetNodeByID(int id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(&root_, id);
}

const BookmarkNode* BookmarkModel::AddGroup(const BookmarkNode* parent,
                                            int index,
                                            const std::wstring& title) {
  if (!loaded_ || parent == &root_ || !IsValidIndex(parent, index, true)) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }

  BookmarkNode* new_node = new BookmarkNode(generate_next_node_id(),
                                            GURL());
  new_node->set_date_group_modified(Time::Now());
  new_node->SetTitle(title);
  new_node->SetType(BookmarkNode::FOLDER);

  return AddNode(AsMutable(parent), index, new_node, false);
}

const BookmarkNode* BookmarkModel::AddURL(const BookmarkNode* parent,
                                          int index,
                                          const std::wstring& title,
                                          const GURL& url) {
  return AddURLWithCreationTime(parent, index, title, url, Time::Now());
}

const BookmarkNode* BookmarkModel::AddURLWithCreationTime(
    const BookmarkNode* parent,
    int index,
    const std::wstring& title,
    const GURL& url,
    const Time& creation_time) {
  if (!loaded_ || !url.is_valid() || is_root(parent) ||
      !IsValidIndex(parent, index, true)) {
    NOTREACHED();
    return NULL;
  }

  bool was_bookmarked = IsBookmarked(url);

  SetDateGroupModified(parent, creation_time);

  BookmarkNode* new_node = new BookmarkNode(generate_next_node_id(), url);
  new_node->SetTitle(title);
  new_node->set_date_added(creation_time);
  new_node->SetType(BookmarkNode::URL);

  {
    // Only hold the lock for the duration of the insert.
    AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node);
  }

  return AddNode(AsMutable(parent), index, new_node, was_bookmarked);
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  if (!parent || !parent->is_folder() || is_root(parent) ||
      parent->GetChildCount() <= 1) {
    return;
  }

  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<Collator> collator(
      Collator::createInstance(
          Locale(g_browser_process->GetApplicationLocale().c_str()),
          error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  BookmarkNode* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children().begin(),
            mutable_parent->children().end(),
            SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChildrenReordered(this, parent));
}

void BookmarkModel::SetURLStarred(const GURL& url,
                                  const std::wstring& title,
                                  bool is_starred) {
  std::vector<const BookmarkNode*> bookmarks;
  GetNodesByURL(url, &bookmarks);
  bool bookmarks_exist = !bookmarks.empty();
  if (is_starred == bookmarks_exist)
    return;  // Nothing to do, state already matches.

  if (is_starred) {
    // Create a bookmark.
    const BookmarkNode* parent = GetParentForNewNodes();
    AddURL(parent, parent->GetChildCount(), title, url);
  } else {
    // Remove all the bookmarks.
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      const BookmarkNode* node = bookmarks[i];
      Remove(node->GetParent(), node->GetParent()->IndexOfChild(node));
    }
  }
}

void BookmarkModel::ResetDateGroupModified(const BookmarkNode* node) {
  SetDateGroupModified(node, Time());
}

void BookmarkModel::GetBookmarksWithTitlesMatching(
    const std::wstring& text,
    size_t max_count,
    std::vector<bookmark_utils::TitleMatch>* matches) {
  if (!loaded_)
    return;

  index_->GetBookmarksWithTitlesMatching(text, max_count, matches);
}

void BookmarkModel::ClearStore() {
  registrar_.RemoveAll();
  store_ = NULL;
}

void BookmarkModel::SetPersistIDs(bool value) {
  if (value == persist_ids_)
    return;
  persist_ids_ = value;
  if (profile_) {
    PrefService* pref_service = profile_->GetPrefs();
    pref_service->SetBoolean(kPrefPersistIDs, persist_ids_);
  }
  // Need to save the bookmark data if the value of persist IDs changes.
  if (store_.get())
    store_->ScheduleSave();
}

bool BookmarkModel::IsBookmarkedNoLock(const GURL& url) {
  BookmarkNode tmp_node(url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void BookmarkModel::FavIconLoaded(const BookmarkNode* node) {
  // Send out notification to the observer.
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeFavIconLoaded(this, node));
}

void BookmarkModel::RemoveNode(BookmarkNode* node,
                               std::set<GURL>* removed_urls) {
  if (!loaded_ || !node || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  if (node->GetType() == BookmarkNode::URL) {
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

    index_->Remove(node);
  }

  CancelPendingFavIconLoadRequests(node);

  // Recurse through children.
  for (int i = node->GetChildCount() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i), removed_urls);
}

void BookmarkModel::DoneLoading(
    BookmarkStorage::LoadDetails* details_delete_me) {
  DCHECK(details_delete_me);
  scoped_ptr<BookmarkStorage::LoadDetails> details(details_delete_me);
  if (loaded_) {
    // We should only ever be loaded once.
    NOTREACHED();
    return;
  }

  bookmark_bar_node_ = details->bb_node();
  other_node_ = details->other_folder_node();
  next_node_id_ = details->max_id();
  if (details->computed_checksum() != details->stored_checksum())
    SetFileChanged();
  index_.reset(details->index());
  details->release();

  // WARNING: order is important here, various places assume bookmark bar then
  // other node.
  root_.Add(0, bookmark_bar_node_);
  root_.Add(1, other_node_);

  {
    AutoLock url_lock(url_lock_);
    // Update nodes_ordered_by_url_set_ from the nodes.
    PopulateNodesByURL(&root_);
  }

  loaded_ = true;

  loaded_signal_.Signal();

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

  BookmarkNode* parent = AsMutable(node->GetParent());
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

  index_->Add(node);

  if (node->GetType() == BookmarkNode::URL && !was_bookmarked) {
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
  loaded_signal_.Wait();
}

const BookmarkNode* BookmarkModel::GetNodeByID(const BookmarkNode* node,
                                               int id) {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->GetChildCount(); i < child_count; ++i) {
    const BookmarkNode* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool BookmarkModel::IsValidIndex(const BookmarkNode* parent,
                                 int index,
                                 bool allow_end) {
  return (parent && parent->is_folder() &&
          (index >= 0 && (index < parent->GetChildCount() ||
                          (allow_end && index == parent->GetChildCount()))));
}

void BookmarkModel::SetDateGroupModified(const BookmarkNode* parent,
                                         const Time time) {
  DCHECK(parent);
  AsMutable(parent)->set_date_group_modified(time);

  if (store_.get())
    store_->ScheduleSave();
}

BookmarkNode* BookmarkModel::CreateBookmarkNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  return CreateRootNodeFromStarredEntry(entry);
}

BookmarkNode* BookmarkModel::CreateOtherBookmarksNode() {
  history::StarredEntry entry;
  entry.type = history::StarredEntry::OTHER;
  return CreateRootNodeFromStarredEntry(entry);
}

BookmarkNode* BookmarkModel::CreateRootNodeFromStarredEntry(
    const history::StarredEntry& entry) {
  DCHECK(entry.type == history::StarredEntry::BOOKMARK_BAR ||
         entry.type == history::StarredEntry::OTHER);
  BookmarkNode* node = new BookmarkNode(generate_next_node_id(), GURL());
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
  node->set_favicon_load_handle(0);
  if (know_favicon && data.get() &&
      PNGDecoder::Decode(&data->data, &fav_icon)) {
    node->set_favicon(fav_icon);
    FavIconLoaded(node);
  }
}

void BookmarkModel::LoadFavIcon(BookmarkNode* node) {
  if (node->GetType() != BookmarkNode::URL)
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
  node->set_favicon_load_handle(handle);
}

void BookmarkModel::CancelPendingFavIconLoadRequests(BookmarkNode* node) {
  if (node->favicon_load_handle()) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->CancelRequest(node->favicon_load_handle());
    node->set_favicon_load_handle(0);
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
        std::vector<const BookmarkNode*> nodes;
        GetNodesByURL(*i, &nodes);
        for (size_t i = 0; i < nodes.size(); ++i) {
          // Got an updated favicon, for a URL, do a new request.
          BookmarkNode* node = AsMutable(nodes[i]);
          node->InvalidateFavicon();
          CancelPendingFavIconLoadRequests(node);
          FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                            BookmarkNodeChanged(this, node));
        }
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

int BookmarkModel::generate_next_node_id() {
  return next_node_id_++;
}

void BookmarkModel::SetFileChanged() {
  file_changed_ = true;
  // If bookmarks file changed externally, the IDs may have changed externally.
  // in that case, the decoder may have reassigned IDs to make them unique.
  // So when the file has changed externally and IDs are persisted, we should
  // save the bookmarks file to persist new IDs.
  if (persist_ids_ && store_.get())
    store_->ScheduleSave();
}

BookmarkStorage::LoadDetails* BookmarkModel::CreateLoadDetails() {
  BookmarkNode* bb_node = CreateBookmarkNode();
  BookmarkNode* other_folder_node = CreateOtherBookmarksNode();
  return new BookmarkStorage::LoadDetails(
      bb_node, other_folder_node, new BookmarkIndex(), next_node_id_);
}

void BookmarkModel::RegisterPreferences() {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  if (!pref_service->IsPrefRegistered(kPrefPersistIDs))
    pref_service->RegisterBooleanPref(kPrefPersistIDs, false);
}

void BookmarkModel::LoadPreferences() {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  persist_ids_ = pref_service->GetBoolean(kPrefPersistIDs);
}
