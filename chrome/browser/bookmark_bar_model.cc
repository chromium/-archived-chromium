// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/bookmark_bar_model.h"

#include "base/gfx/png_decoder.h"
#include "chrome/browser/profile.h"
#include "generated_resources.h"

// BookmarkBarNode ------------------------------------------------------------

const SkBitmap& BookmarkBarNode::GetFavIcon() {
  if (!loaded_favicon_) {
    loaded_favicon_ = true;
    model_->LoadFavIcon(this);
  }
  return favicon_;
}

BookmarkBarNode::BookmarkBarNode(BookmarkBarModel* model)
    : model_(model),
      group_id_(0),
      star_id_(0),
      loaded_favicon_(false),
      favicon_load_handle_(0),
      type_(history::StarredEntry::BOOKMARK_BAR),
      date_added_(Time::Now()) {
  DCHECK(model_);
}

void BookmarkBarNode::Reset(const history::StarredEntry& entry) {
  // We should either have no id, or the id of the new entry should match
  // this.
  DCHECK(!star_id_ || star_id_ == entry.id);
  star_id_ = entry.id;
  group_id_ = entry.group_id;
  url_ = entry.url;
  favicon_ = SkBitmap();
  loaded_favicon_ = false;
  favicon_load_handle_ = 0;
  type_ = entry.type;
  date_added_ = entry.date_added;
  date_group_modified_ = entry.date_group_modified;
  SetTitle(entry.title);
}

void BookmarkBarNode::SetURL(const GURL& url) {
  DCHECK(favicon_load_handle_ == 0);
  loaded_favicon_ = false;
  favicon_load_handle_ = 0;
  url_ = url;
  favicon_ .reset();
}

history::StarredEntry BookmarkBarNode::GetEntry() {
  history::StarredEntry entry;
  entry.id = GetStarID();
  entry.group_id = group_id_;
  entry.url = GetURL();
  entry.title = GetTitle();
  entry.type = type_;
  entry.date_added = date_added_;
  entry.date_group_modified = date_group_modified_;
  // Only set the parent and visual order if we have a valid parent (the root
  // node is not in the db and has a group_id of 0).
  if (GetParent() && GetParent()->group_id_) {
    entry.visual_order = GetParent()->IndexOfChild(this);
    entry.parent_group_id = GetParent()->GetGroupID();
  }
  return entry;
}

// BookmarkBarModel -----------------------------------------------------------

BookmarkBarModel::BookmarkBarModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      root_(this),
      // See declaration for description.
      next_group_id_(HistoryService::kBookmarkBarID + 1),
      bookmark_bar_node_(NULL),
      other_node_(NULL) {
  // Notifications we want.
  if (profile_)
    NotificationService::current()->AddObserver(
        this, NOTIFY_STARRED_FAVICON_CHANGED, Source<Profile>(profile_));

  if (!profile || !profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)) {
    // Profile/HistoryService is NULL during testing.
    CreateBookmarkBarNode();
    CreateOtherBookmarksNode();
    AddRootChildren(NULL);
    loaded_ = true;
    return;
  }

  // Request the entries on the bookmark bar.
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->
      GetAllStarredEntries(&load_consumer_,
                           NewCallback(this,
                                       &BookmarkBarModel::OnGotStarredEntries));
}

BookmarkBarModel::~BookmarkBarModel() {
  if (profile_) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_STARRED_FAVICON_CHANGED, Source<Profile>(profile_));
  }
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

void BookmarkBarModel::Remove(BookmarkBarNode* parent, int index) {
#ifndef NDEBUG
  CheckIndex(parent, index, false);
#endif
  if (parent != &root_) {
    RemoveAndDeleteNode(parent->GetChild(index));
  } else {
    NOTREACHED();  // Can't remove from the root.
  }
}

void BookmarkBarModel::RemoveFromBookmarkBar(BookmarkBarNode* node) {
  if (!node->HasAncestor(bookmark_bar_node_))
    return;

  if (node != &root_ && node != bookmark_bar_node_ && node != other_node_) {
    Move(node, other_node_, other_node_->GetChildCount());
  } else {
    NOTREACHED();  // Can't move the root, bookmark bar or other nodes.
  }
}

void BookmarkBarModel::Move(BookmarkBarNode* node,
                            BookmarkBarNode* new_parent,
                            int index) {
  DCHECK(node && new_parent);
  DCHECK(node->GetParent());
#ifndef NDEBUG
  CheckIndex(new_parent, index, true);
#endif

  if (new_parent == &root_ || node == &root_ || node == bookmark_bar_node_ ||
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

  HistoryService* history = !profile_ ? NULL :
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history)
    history->UpdateStarredEntry(node->GetEntry());
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeMoved(this, old_parent, old_index,
                                      new_parent, index));
}

void BookmarkBarModel::SetTitle(BookmarkBarNode* node,
                                const std::wstring& title) {
  DCHECK(node);
  if (node->GetTitle() == title)
    return;
  node->SetTitle(title);
  HistoryService* history = !profile_ ? NULL :
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history)
    history->UpdateStarredEntry(node->GetEntry());
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

BookmarkBarNode* BookmarkBarModel::GetNodeByURL(const GURL& url) {
  BookmarkBarNode tmp_node(this);
  tmp_node.url_ = url;
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  return (i != nodes_ordered_by_url_set_.end()) ? *i : NULL;
}

BookmarkBarNode* BookmarkBarModel::GetNodeByGroupID(
    history::UIStarID group_id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByGroupID(&root_, group_id);
}

BookmarkBarNode* BookmarkBarModel::AddGroup(
    BookmarkBarNode* parent,
    int index,
    const std::wstring& title) {
  DCHECK(IsLoaded());
#ifndef NDEBUG
  CheckIndex(parent, index, true);
#endif
  if (parent == &root_) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }

  BookmarkBarNode* new_node = new BookmarkBarNode(this);
  new_node->group_id_ = next_group_id_++;
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
  DCHECK(IsLoaded() && url.is_valid() && parent);
  if (parent == &root_) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }
#ifndef NDEBUG
  CheckIndex(parent, index, true);
#endif

  BookmarkBarNode* existing_node = GetNodeByURL(url);
  if (existing_node) {
    Move(existing_node, parent, index);
    SetTitle(existing_node, title);
    return existing_node;
  }

  SetDateGroupModified(parent, creation_time);

  BookmarkBarNode* new_node = new BookmarkBarNode(this);
  new_node->SetTitle(title);
  new_node->SetURL(url);
  new_node->date_added_ = creation_time;
  new_node->type_ = history::StarredEntry::URL;

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

void BookmarkBarModel::RemoveNode(BookmarkBarNode* node) {
  DCHECK(node && node != bookmark_bar_node_ && node != other_node_);
  if (node->GetType() == history::StarredEntry::URL) {
    NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(node);
    DCHECK(i != nodes_ordered_by_url_set_.end());
    nodes_ordered_by_url_set_.erase(i);
  }

  NodeToHandleMap::iterator i = node_to_handle_map_.find(node);
  if (i != node_to_handle_map_.end()) {
    HistoryService* history = !profile_ ? NULL :
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      request_consumer_.SetClientData(history, i->second, NULL);
    node_to_handle_map_.erase(i);
  }

  CancelPendingFavIconLoadRequests(node);

  // Recurse through children.
  for (int i = node->GetChildCount() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i));
}

void BookmarkBarModel::OnGotStarredEntries(
    HistoryService::Handle,
    std::vector<history::StarredEntry>* entries) {
  if (loaded_) {
    NOTREACHED();
    return;
  }

  DCHECK(entries);
  // Create tree nodes for each of the elements.
  PopulateNodes(entries);

  // Yes, we've finished loading.
  loaded_ = true;

  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_, Loaded(this));

  NotificationService::current()->Notify(
      NOTIFY_BOOKMARK_MODEL_LOADED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

void BookmarkBarModel::PopulateNodes(
    std::vector<history::StarredEntry>* entries) {
  std::map<history::UIStarID,history::StarID> group_id_to_id_map;
  IDToNodeMap id_to_node_map;

  // Iterate through the entries building a mapping between group_id and id as
  // well as creating the bookmark bar node and other node.
  for (std::vector<history::StarredEntry>::const_iterator i = entries->begin();
       i != entries->end(); ++i) {
    if (i->type == history::StarredEntry::URL)
      continue;

    if (i->type == history::StarredEntry::OTHER)
      other_node_ = CreateRootNodeFromStarredEntry(*i);
    else if (i->type == history::StarredEntry::BOOKMARK_BAR)
      bookmark_bar_node_ = CreateRootNodeFromStarredEntry(*i);

    group_id_to_id_map[i->group_id] = i->id;
  }

  // Add the bookmark bar and other nodes to the root node.
  AddRootChildren(&id_to_node_map);

  // If the db was corrupt and we didn't get the bookmark bar/other nodes,
  // AddRootChildren will create them. Update the map to make sure it includes
  // these nodes.
  group_id_to_id_map[other_node_->group_id_] = other_node_->star_id_;
  group_id_to_id_map[bookmark_bar_node_->group_id_] =
      bookmark_bar_node_->star_id_;

  // Iterate through the entries again creating the nodes.
  for (std::vector<history::StarredEntry>::iterator i = entries->begin();
       i != entries->end(); ++i) {
    if (!i->parent_group_id) {
      DCHECK(i->type == history::StarredEntry::BOOKMARK_BAR ||
             i->type == history::StarredEntry::OTHER);
      // Ignore entries not parented to the bookmark bar.
      continue;
    }

    BookmarkBarNode* node = id_to_node_map[i->id];
    if (!node) {
      // Creating a node results in creating the parent. As such, it is
      // possible for the node representing a group to have been created before
      // encountering the details.

      // The created nodes are owned by the root node.
      node = new BookmarkBarNode(this);
      id_to_node_map[i->id] = node;
    }
    node->Reset(*i);

    DCHECK(group_id_to_id_map.find(i->parent_group_id) !=
           group_id_to_id_map.end());
    history::StarID parent_id = group_id_to_id_map[i->parent_group_id];
    BookmarkBarNode* parent = id_to_node_map[parent_id];
    if (!parent) {
      // Haven't encountered the parent yet, create it now.
      parent = new BookmarkBarNode(this);
      id_to_node_map[parent_id] = parent;
    }

    if (i->type == history::StarredEntry::URL)
      nodes_ordered_by_url_set_.insert(node);
    else
      next_group_id_ = std::max(next_group_id_, i->group_id + 1);

    // Add the node to its parent. entries is ordered by parent then
    // visual order so that we know we maintain visual order by always adding
    // to the end.
    parent->Add(parent->GetChildCount(), node);
  }
}

void BookmarkBarModel::OnCreatedEntry(HistoryService::Handle handle,
                                      history::StarID id) {
  BookmarkBarNode* node = request_consumer_.GetClientData(
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS), handle);
  // Node is NULL if the node was removed by the user before the request
  // was processed.
  if (node) {
    DCHECK(!node->star_id_);
    node->star_id_ = id;
    DCHECK(node_to_handle_map_.find(node) != node_to_handle_map_.end());
    node_to_handle_map_.erase(node_to_handle_map_.find(node));
  }
}

void BookmarkBarModel::RemoveAndDeleteNode(BookmarkBarNode* delete_me) {
  scoped_ptr<BookmarkBarNode> node(delete_me);

  BookmarkBarNode* parent = node->GetParent();
  DCHECK(parent);
  int index = parent->IndexOfChild(node.get());
  parent->Remove(index);
  RemoveNode(node.get());

  HistoryService* history = profile_ ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : NULL;
  if (history) {
    if (node->GetType() == history::StarredEntry::URL) {
      history->DeleteStarredURL(node->GetURL());
    } else {
      history->DeleteStarredGroup(node->GetGroupID());
    }
  }
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeRemoved(this, parent, index));
}

BookmarkBarNode* BookmarkBarModel::AddNode(BookmarkBarNode* parent,
    int index,
    BookmarkBarNode* node) {
  parent->Add(index, node);

  // NOTE: As history calls us back when we invoke CreateStarredEntry, we have
  // to be sure to invoke it after we've updated the nodes appropriately.
  HistoryService* history = !profile_ ? NULL :
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history) {
    HistoryService::Handle handle =
        history->CreateStarredEntry(node->GetEntry(), &request_consumer_,
            NewCallback(this, &BookmarkBarModel::OnCreatedEntry));
    request_consumer_.SetClientData(history, handle, node);
    node_to_handle_map_[node] = handle;
  }
  FOR_EACH_OBSERVER(BookmarkBarModelObserver, observers_,
                    BookmarkNodeAdded(this, parent, index));
  return node;
}

BookmarkBarNode* BookmarkBarModel::GetNodeByGroupID(
    BookmarkBarNode* node,
    history::UIStarID group_id) {
  if (node->GetType() == history::StarredEntry::USER_GROUP &&
      node->GetGroupID() == group_id) {
    return node;
  }
  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkBarNode* result = GetNodeByGroupID(node->GetChild(i), group_id);
    if (result)
      return result;
  }
  return NULL;
}

void BookmarkBarModel::SetDateGroupModified(BookmarkBarNode* parent,
                                            const Time time) {
  DCHECK(parent);
  parent->date_group_modified_ = time;
  HistoryService* history = profile_ ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : NULL;
  if (history)
    history->UpdateStarredEntry(parent->GetEntry());
}

void BookmarkBarModel::CreateBookmarkBarNode() {
  history::StarredEntry entry;
  entry.id = HistoryService::kBookmarkBarID;
  entry.group_id = HistoryService::kBookmarkBarID;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  bookmark_bar_node_ = CreateRootNodeFromStarredEntry(entry);
}

void BookmarkBarModel::CreateOtherBookmarksNode() {
  history::StarredEntry entry;
  entry.id = HistoryService::kBookmarkBarID;
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    entry.id = std::max(entry.id, (*i)->GetStarID());
  }
  entry.id++;
  entry.group_id = next_group_id_++;
  entry.type = history::StarredEntry::OTHER;
  other_node_ = CreateRootNodeFromStarredEntry(entry);
}

BookmarkBarNode* BookmarkBarModel::CreateRootNodeFromStarredEntry(
    const history::StarredEntry& entry) {
  DCHECK(entry.type == history::StarredEntry::BOOKMARK_BAR ||
         entry.type == history::StarredEntry::OTHER);
  BookmarkBarNode* node = new BookmarkBarNode(this);
  node->Reset(entry);
  if (entry.type == history::StarredEntry::BOOKMARK_BAR)
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  else
    node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));
  next_group_id_ = std::max(next_group_id_, entry.group_id + 1);
  return node;
}

void BookmarkBarModel::AddRootChildren(IDToNodeMap* id_to_node_map) {
  // When invoked we should have created the bookmark bar and other nodes. If
  // not, it indicates the db couldn't be loaded correctly or is corrupt.
  // Force creation so that bookmark bar model is still usable.
  if (!bookmark_bar_node_) {
    LOG(WARNING) << "No bookmark bar entry in the database. This indicates "
                    "bookmarks database couldn't be loaded or is corrupt.";
    CreateBookmarkBarNode();
  }
  if (!other_node_) {
    LOG(WARNING) << "No other folders bookmark bar entry in the database. This "
                    "indicates bookmarks database couldn't be loaded or is "
                    "corrupt.";
    CreateOtherBookmarksNode();
  }

  if (id_to_node_map) {
    (*id_to_node_map)[other_node_->GetStarID()] = other_node_;
    (*id_to_node_map)[bookmark_bar_node_->GetStarID()] = bookmark_bar_node_;
  }

  // WARNING: order is important here, various places assume bookmark bar then
  // other node.
  root_.Add(0, bookmark_bar_node_);
  root_.Add(1, other_node_);
}

void BookmarkBarModel::OnFavIconDataAvailable(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  SkBitmap fav_icon;
  if (know_favicon && data.get() &&
      PNGDecoder::Decode(&data->data, &fav_icon)) {
    BookmarkBarNode* node =
        load_consumer_.GetClientData(
            profile_->GetHistoryService(Profile::EXPLICIT_ACCESS), handle);
    DCHECK(node);
    node->favicon_ = fav_icon;
    node->favicon_load_handle_ = 0;
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

// static
bool BookmarkBarModel::MoreRecentlyModified(BookmarkBarNode* n1,
                                            BookmarkBarNode* n2) {
  return n1->date_group_modified_ > n2->date_group_modified_;
}

void BookmarkBarModel::GetMostRecentlyModifiedGroupNodes(
    BookmarkBarNode* parent,
    size_t count,
    std::vector<BookmarkBarNode*>* nodes) {
  if (parent->group_id_ && parent->date_group_modified_ > Time()) {
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
    if (child->GetType() != history::StarredEntry::URL) {
      GetMostRecentlyModifiedGroupNodes(child, count, nodes);
    }
  }
}

void BookmarkBarModel::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_STARRED_FAVICON_CHANGED: {
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

    default:
      NOTREACHED();
      break;
  }
}
