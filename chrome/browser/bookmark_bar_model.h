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

#ifndef CHROME_BROWSER_BOOKMARK_BAR_MODEL_H_
#define CHROME_BROWSER_BOOKMARK_BAR_MODEL_H_

#include "base/observer_list.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/tree_node_model.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

class BookmarkBarModel;
class BookmarkCodec;
class Profile;

// BookmarkBarNode ------------------------------------------------------------

// BookmarkBarNode contains information about a starred entry: title, URL,
// favicon, star id and type. BookmarkBarNodes are returned from a
// BookmarkBarModel.
//
class BookmarkBarNode : public ChromeViews::TreeNode<BookmarkBarNode> {
  friend class BookmarkBarModel;
  friend class BookmarkCodec;

 public:
  explicit BookmarkBarNode(BookmarkBarModel* model);

  virtual ~BookmarkBarNode() {}

  // Returns the favicon for the this node. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const SkBitmap& GetFavIcon();

  // Returns the URL.
  const GURL& GetURL() const { return url_; }

  // Returns the start ID corresponding to this node.
  // TODO(sky): bug 1256202, make this an ever increasing integer assigned on
  // reading, but not archived. Best to set it automatically in the constructor.
  history::StarID GetStarID() const { return star_id_; }

  // Returns the type of this node.
  history::StarredEntry::Type GetType() const { return type_; }

  // Returns a StarredEntry for the node.
  history::StarredEntry GetEntry();

  // Returns the ID of group.
  // TODO(sky): bug 1256202, nuke this.
  history::UIStarID GetGroupID() { return group_id_; }

  // Called when the favicon becomes invalid.
  void InvalidateFavicon() {
    loaded_favicon_ = false;
    favicon_ = SkBitmap();
  }

  // Returns the time the bookmark/group was added.
  Time date_added() const { return date_added_; }

  // Returns the last time the group was modified. This is only maintained
  // for folders (including the bookmark and other folder).
  Time date_group_modified() const { return date_group_modified_; }

 private:
  // Resets the properties of the node from the supplied entry.
  void Reset(const history::StarredEntry& entry);

  // Resets the URL. Take care to cancel loading before invoking this.
  void SetURL(const GURL& url);

  // The model.
  BookmarkBarModel* model_;

  // Unique identifier for this node.
  history::StarID star_id_;

  // Whether the favicon has been loaded.
  bool loaded_favicon_;

  // The favicon.
  SkBitmap favicon_;

  // If non-zero, it indicates we're loading the favicon and this is the handle
  // from the HistoryService.
  HistoryService::Handle favicon_load_handle_;

  // URL.
  GURL url_;

  // Type of node.
  // TODO(sky): bug 1256202, convert this into a type defined here.
  history::StarredEntry::Type type_;

  // Group ID.
  history::UIStarID group_id_;

  // Date we were created.
  Time date_added_;

  // Time last modified. Only used for groups.
  Time date_group_modified_;

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkBarNode);
};


// BookmarkBarModelObserver ---------------------------------------------------

// Observer for the BookmarkBarModel.
//
class BookmarkBarModelObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void Loaded(BookmarkBarModel* model) = 0;

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(BookmarkBarModel* model,
                                 BookmarkBarNode* old_parent,
                                 int old_index,
                                 BookmarkBarNode* new_parent,
                                 int new_index) = 0;

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkBarModel* model,
                                 BookmarkBarNode* parent,
                                 int index) = 0;

  // Invoked when a node has been removed, the item may still be starred though.
  virtual void BookmarkNodeRemoved(BookmarkBarModel* model,
                                   BookmarkBarNode* parent,
                                   int index) = 0;

  // Invoked when the title or favicon of a node has changed.
  virtual void BookmarkNodeChanged(BookmarkBarModel* model,
                                   BookmarkBarNode* node) = 0;

  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFavIconLoaded(BookmarkBarModel* model,
                                         BookmarkBarNode* node) = 0;
};

// BookmarkBarModel -----------------------------------------------------------

// BookmarkBarModel provides a directed acyclic graph of the starred entries
// and groups. Two graphs are provided for the two entry points: those on
// the bookmark bar, and those in the other folder.
//
// The methods of BookmarkBarModel update the internal structure immediately
// and update the backend in the background.
//
// An observer may be attached to observer relevant events.
//
// You should NOT directly create a BookmarkBarModel, instead go through the
// Profile.

// TODO(sky): rename to BookmarkModel.
class BookmarkBarModel : public NotificationObserver {
  friend class BookmarkBarNode;
  friend class BookmarkBarModelTest;

 public:
  explicit BookmarkBarModel(Profile* profile);
  virtual ~BookmarkBarModel();

  // Returns the root node. The bookmark bar node and other node are children of
  // the root node.
  BookmarkBarNode* root_node() { return &root_; }

  // Returns the bookmark bar node.
  BookmarkBarNode* GetBookmarkBarNode() { return bookmark_bar_node_; }

  // Returns the 'other' node.
  BookmarkBarNode* other_node() { return other_node_; }

  // Returns the parent the last node was added to. This never returns NULL
  // (as long as the model is loaded).
  BookmarkBarNode* GetParentForNewNodes();

  // Returns a vector containing up to |max_count| of the most recently
  // modified groups. This never returns an empty vector.
  std::vector<BookmarkBarNode*> GetMostRecentlyModifiedGroups(size_t max_count);

  void AddObserver(BookmarkBarModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BookmarkBarModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Unstars or deletes the specified entry. Removing a group entry recursively
  // unstars all nodes. Observers are notified immediately.
  void Remove(BookmarkBarNode* parent, int index);

  // If the specified node is on the bookmark bar it is removed from the
  // bookmark bar to be a child of the other node. If the node is not on the
  // bookmark bar this does nothing. This is a convenience for invoking
  // Move to the other node.
  void RemoveFromBookmarkBar(BookmarkBarNode* node);

  // Moves the specified entry to a new location.
  void Move(BookmarkBarNode* node, BookmarkBarNode* new_parent, int index);

  // Sets the title of the specified node.
  void SetTitle(BookmarkBarNode* node, const std::wstring& title);

  // Returns true if the model finished loading.
  bool IsLoaded() { return loaded_; }

  // Returns the node with the specified URL, or NULL if there is no node with
  // the specified URL.
  BookmarkBarNode* GetNodeByURL(const GURL& url);

  // Returns the node with the specified group id, or NULL if there is no node
  // with the specified id.
  BookmarkBarNode* GetNodeByGroupID(history::UIStarID group_id);

  // Adds a new group node at the specified position.
  BookmarkBarNode* AddGroup(BookmarkBarNode* parent,
                            int index,
                            const std::wstring& title);

  // Adds a url at the specified position. If there is already a node with the
  // specified URL, it is moved to the new position.
  BookmarkBarNode* AddURL(BookmarkBarNode* parent,
                          int index,
                          const std::wstring& title,
                          const GURL& url);

  // Adds a url with a specific creation date.
  BookmarkBarNode* AddURLWithCreationTime(BookmarkBarNode* parent,
                                          int index,
                                          const std::wstring& title,
                                          const GURL& url,
                                          const Time& creation_time);

  // This is the convenience that makes sure the url is starred or not
  // starred. If the URL is not currently starred, it is added to the
  // most recent parent.
  void SetURLStarred(const GURL& url,
                     const std::wstring& title,
                     bool is_starred);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created groups from showing up in the
  // combobox of most recently modified groups.
  void ResetDateGroupModified(BookmarkBarNode* node);

 private:
  // Used to order BookmarkBarNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(BookmarkBarNode* n1, BookmarkBarNode* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Maps from star id of node to actual node. Used during creation of nodes
  // from history::StarredEntries.
  typedef std::map<history::StarID,BookmarkBarNode*> IDToNodeMap;

  // Overriden to notify the observer the favicon has been loaded.
  void FavIconLoaded(BookmarkBarNode* node);

  // NOTE: this does NOT override EntryAdded/EntryChanged as we assume all
  // mutation to entries on the bookmark bar occurs through our methods.

  // Removes the node for internal maps. This does NOT delete the node.
  void RemoveNode(BookmarkBarNode* node);

  // Callback from the database with the starred entries. Adds the appropriate
  // entries to the root node.
  void OnGotStarredEntries(HistoryService::Handle,
      std::vector<history::StarredEntry>* entries);

  // Invoked from OnGotStarredEntries to create all the BookmarkNodes for
  // the specified entries.
  void PopulateNodes(std::vector<history::StarredEntry>* entries);

  // Callback from AddFolder/AddURL.
  void OnCreatedEntry(HistoryService::Handle handle, history::StarID id);

  // Removes the node from its parent, sends notification, and deletes it.
  // type specifies how the node should be removed.
  void RemoveAndDeleteNode(BookmarkBarNode* delete_me);

  // Adds the node at the specified position, and sends notification.
  BookmarkBarNode* AddNode(BookmarkBarNode* parent,
                           int index,
                           BookmarkBarNode* node);

  // Implementation of GetNodeByGrouID.
  BookmarkBarNode* GetNodeByGroupID(BookmarkBarNode* node,
                                    history::UIStarID group_id);

  // Adds the bookmark bar and other nodes to the root node. If id_to_node_map
  // is non-null, the bookmark bar node and other bookmark nodes are added to
  // it.
  void AddRootChildren(IDToNodeMap* id_to_node_map);

#ifndef NDEBUG
  void CheckIndex(BookmarkBarNode* parent, int index, bool allow_end) {
    DCHECK(parent);
    DCHECK(index >= 0 &&
           (index < parent->GetChildCount() ||
            allow_end && index == parent->GetChildCount()));
  }
#endif

  // Sets the date modified time of the specified node.
  void SetDateGroupModified(BookmarkBarNode* parent, const Time time);

  // Creates the bookmark bar/other nodes. This is used during testing (when we
  // don't load from the DB), as well as if the db doesn't give us back a
  // bookmark bar or other node (which should only happen if there is an error
  // in loading the DB).
  void CreateBookmarkBarNode();
  void CreateOtherBookmarksNode();

  // Creates a root node (either the bookmark bar node or other node) from the
  // specified starred entry. Only used once when loading.
  BookmarkBarNode* CreateRootNodeFromStarredEntry(
      const history::StarredEntry& entry);

  // Notification that a favicon has finished loading. If we can decode the
  // favicon, FaviconLoaded is invoked.
  void OnFavIconDataAvailable(
      HistoryService::Handle handle,
      bool know_favicon,
      scoped_refptr<RefCountedBytes> data,
      bool expired,
      GURL icon_url);

  // Invoked from the node to load the favicon. Requests the favicon from the
  // history service.
  void LoadFavIcon(BookmarkBarNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFavIconLoadRequests(BookmarkBarNode* node);

  // Returns true if n1's date modified time is newer than n2s.
  static bool MoreRecentlyModified(BookmarkBarNode* n1, BookmarkBarNode* n2);

  // Returns up to count of the most recently modified groups. This may not
  // add anything.
  void GetMostRecentlyModifiedGroupNodes(BookmarkBarNode* parent,
                                         size_t count,
                                         std::vector<BookmarkBarNode*>* nodes);

  // NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  // Whether the initial set of data has been loaded.
  bool loaded_;

  // The root node. This contains the bookmark bar node and the 'other' node as
  // children.
  BookmarkBarNode root_;

  BookmarkBarNode* bookmark_bar_node_;
  BookmarkBarNode* other_node_;

  // The observers.
  ObserverList<BookmarkBarModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  typedef std::set<BookmarkBarNode*,NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;

  // Maps from node to request handle. This is used when creating new nodes.
  typedef std::map<BookmarkBarNode*,HistoryService::Handle> NodeToHandleMap;
  NodeToHandleMap node_to_handle_map_;

  // Used when creating new entries/groups. Maps to the newly created
  // node.
  CancelableRequestConsumerT<BookmarkBarNode*,NULL> request_consumer_;

  // ID of the next group we create.
  //
  // After the first load, this is set to the max group_id from the database.
  //
  // This value is incremented every time a new group is created. It is
  // important that the value assigned to a group node remain unique during
  // a run of Chrome (BookmarkEditorView for one relies on this).
  history::StarID next_group_id_;

  // Used for loading favicons.
  CancelableRequestConsumerT<BookmarkBarNode*, NULL> load_consumer_;

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkBarModel);
};

#endif  // CHROME_BROWSER_BOOKMARK_BAR_MODEL_H_
