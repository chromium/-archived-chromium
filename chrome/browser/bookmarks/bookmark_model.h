// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_MODEL_H_

#include "build/build_config.h"

#include "base/lock.h"
#include "base/observer_list.h"
#include "base/waitable_event.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/views/tree_node_model.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

class BookmarkEditorView;
class BookmarkModel;
class BookmarkCodec;
class Profile;

namespace history {
class StarredURLDatabase;
}

// BookmarkNode ---------------------------------------------------------------

// BookmarkNode contains information about a starred entry: title, URL, favicon,
// star id and type. BookmarkNodes are returned from a BookmarkModel.
//
class BookmarkNode : public views::TreeNode<BookmarkNode> {
  friend class BookmarkEditorView;
  friend class BookmarkModel;
  friend class BookmarkCodec;
  friend class history::StarredURLDatabase;
  FRIEND_TEST(BookmarkEditorViewTest, ChangeParentAndURL);
  FRIEND_TEST(BookmarkEditorViewTest, EditURLKeepsPosition);
  FRIEND_TEST(BookmarkModelTest, MostRecentlyAddedEntries);
  FRIEND_TEST(BookmarkModelTest, GetMostRecentlyAddedNodeForURL);

 public:
  BookmarkNode(BookmarkModel* model, const GURL& url);
  virtual ~BookmarkNode() {}

  // Returns the favicon for the this node. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const SkBitmap& GetFavIcon();

  // Returns the URL.
  const GURL& GetURL() const { return url_; }

  // Returns a unique id for this node.
  //
  // NOTE: this id is only unique for the session and NOT unique across
  // sessions. Don't persist it!
  int id() const { return id_; }

  // Returns the type of this node.
  history::StarredEntry::Type GetType() const { return type_; }

  // Called when the favicon becomes invalid.
  void InvalidateFavicon() {
    loaded_favicon_ = false;
    favicon_ = SkBitmap();
  }

  // Returns the time the bookmark/group was added.
  base::Time date_added() const { return date_added_; }

  // Returns the last time the group was modified. This is only maintained
  // for folders (including the bookmark and other folder).
  base::Time date_group_modified() const { return date_group_modified_; }

  // Convenience for testing if this nodes represents a group. A group is
  // a node whose type is not URL.
  bool is_folder() const { return type_ != history::StarredEntry::URL; }

  // Is this a URL?
  bool is_url() const { return type_ == history::StarredEntry::URL; }

  // TODO(sky): Consider adding last visit time here, it'll greatly simplify
  // HistoryContentsProvider.

 private:
  // Resets the properties of the node from the supplied entry.
  void Reset(const history::StarredEntry& entry);

  // The model. This is NULL when created by StarredURLDatabase for migration.
  BookmarkModel* model_;

  // Unique identifier for this node.
  const int id_;

  // Whether the favicon has been loaded.
  bool loaded_favicon_;

  // The favicon.
  SkBitmap favicon_;

  // If non-zero, it indicates we're loading the favicon and this is the handle
  // from the HistoryService.
  HistoryService::Handle favicon_load_handle_;

  // The URL. BookmarkModel maintains maps off this URL, it is important that
  // it not change once the node has been created.
  const GURL url_;

  // Type of node.
  // TODO(sky): bug 1256202, convert this into a type defined here.
  history::StarredEntry::Type type_;

  // Date we were created.
  base::Time date_added_;

  // Time last modified. Only used for groups.
  base::Time date_group_modified_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNode);
};

// BookmarkModelObserver ------------------------------------------------------

// Observer for the BookmarkModel.
//
class BookmarkModelObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void Loaded(BookmarkModel* model) = 0;

  // Invoked from the destructor of the BookmarkModel.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) { }

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) = 0;

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) = 0;

  // Invoked when a node has been removed, the item may still be starred though.
  // TODO(sky): merge these two into one.
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int old_index,
                                   BookmarkNode* node) {
    BookmarkNodeRemoved(model, parent, old_index);
  }

  // Invoked when the title or favicon of a node has changed.
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) = 0;

  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) = 0;

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node) = 0;
};

// BookmarkModel --------------------------------------------------------------

// BookmarkModel provides a directed acyclic graph of the starred entries
// and groups. Two graphs are provided for the two entry points: those on
// the bookmark bar, and those in the other folder.
//
// An observer may be attached to observer relevant events.
//
// You should NOT directly create a BookmarkModel, instead go through the
// Profile.

class BookmarkModel : public NotificationObserver, public BookmarkService {
  friend class BookmarkNode;
  friend class BookmarkModelTest;
  friend class BookmarkStorage;

 public:
  explicit BookmarkModel(Profile* profile);
  virtual ~BookmarkModel();

  // Loads the bookmarks. This is called by Profile upon creation of the
  // BookmarkModel. You need not invoke this directly.
  void Load();

  // Returns the root node. The bookmark bar node and other node are children of
  // the root node.
  BookmarkNode* root_node() { return &root_; }

  // Returns the bookmark bar node.
  BookmarkNode* GetBookmarkBarNode() { return bookmark_bar_node_; }

  // Returns the 'other' node.
  BookmarkNode* other_node() { return other_node_; }

  // Returns the parent the last node was added to. This never returns NULL
  // (as long as the model is loaded).
  BookmarkNode* GetParentForNewNodes();

  void AddObserver(BookmarkModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BookmarkModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Unstars or deletes the specified entry. Removing a group entry recursively
  // unstars all nodes. Observers are notified immediately.
  void Remove(BookmarkNode* parent, int index);

  // Moves the specified entry to a new location.
  void Move(BookmarkNode* node, BookmarkNode* new_parent, int index);

  // Sets the title of the specified node.
  void SetTitle(BookmarkNode* node, const std::wstring& title);

  // Returns true if the model finished loading.
  bool IsLoaded() { return loaded_; }

  // Returns the set of nodes with the specified URL.
  void GetNodesByURL(const GURL& url, std::vector<BookmarkNode*>* nodes);

  // Returns the most recently added node for the url. Returns NULL if url is
  // not bookmarked.
  BookmarkNode* GetMostRecentlyAddedNodeForURL(const GURL& url);

  // Returns all the bookmarked urls. This method is thread safe.
  virtual void GetBookmarks(std::vector<GURL>* urls);

  // Returns true if there is a bookmark for the specified URL. This method is
  // thread safe. See BookmarkService for more details on this.
  virtual bool IsBookmarked(const GURL& url);

  // Blocks until loaded; this is NOT invoked on the main thread. See
  // BookmarkService for more details on this.
  virtual void BlockTillLoaded();

  // Returns the node with the specified id, or NULL if there is no node with
  // the specified id.
  BookmarkNode* GetNodeByID(int id);

  // Adds a new group node at the specified position.
  BookmarkNode* AddGroup(BookmarkNode* parent,
                         int index,
                         const std::wstring& title);

  // Adds a url at the specified position.
  BookmarkNode* AddURL(BookmarkNode* parent,
                       int index,
                       const std::wstring& title,
                       const GURL& url);

  // Adds a url with a specific creation date.
  BookmarkNode* AddURLWithCreationTime(BookmarkNode* parent,
                                       int index,
                                       const std::wstring& title,
                                       const GURL& url,
                                       const base::Time& creation_time);

  // Sorts the children of |parent|, notifying observers by way of the
  // BookmarkNodeChildrenReordered method.
  void SortChildren(BookmarkNode* parent);

  // This is the convenience that makes sure the url is starred or not starred.
  // If is_starred is false, all bookmarks for URL are removed. If is_starred is
  // true and there are no bookmarks for url, a bookmark is created.
  void SetURLStarred(const GURL& url,
                     const std::wstring& title,
                     bool is_starred);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created groups from showing up in the
  // combobox of most recently modified groups.
  void ResetDateGroupModified(BookmarkNode* node);

  Profile* profile() const { return profile_; }

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

 private:
  // Used to order BookmarkNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(BookmarkNode* n1, BookmarkNode* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Implementation of IsBookmarked. Before calling this the caller must
  // obtain a lock on url_lock_.
  bool IsBookmarkedNoLock(const GURL& url);

  // Overriden to notify the observer the favicon has been loaded.
  void FavIconLoaded(BookmarkNode* node);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNode(BookmarkNode* node, std::set<GURL>* removed_urls);

  // Callback from BookmarkStorage that it has finished loading. This method
  // may be hit twice. In particular, on construction BookmarkModel asks
  // BookmarkStorage to load the bookmarks. BookmarkStorage invokes this method
  // with loaded_from_history false and file_exists indicating whether the
  // bookmarks file exists. If the file doesn't exist, we query history. When
  // history calls us back (OnHistoryDone) we then ask BookmarkStorage to load
  // from the migration file. BookmarkStorage again invokes this method, but
  // with |loaded_from_history| true.
  void OnBookmarkStorageLoadedBookmarks(bool file_exists,
                                        bool loaded_from_history);

  // Used for migrating bookmarks from history to standalone file.
  //
  // Callback from history that it is done with an empty request. This is used
  // if there is no bookmarks file. Once done, we attempt to load from the
  // temporary file creating during migration.
  void OnHistoryDone();

  // Invoked when loading is finished. Sets loaded_ and notifies observers.
  void DoneLoading();

  // Populates nodes_ordered_by_url_set_ from root.
  void PopulateNodesByURL(BookmarkNode* node);

  // Removes the node from its parent, sends notification, and deletes it.
  // type specifies how the node should be removed.
  void RemoveAndDeleteNode(BookmarkNode* delete_me);

  // Adds the node at the specified position and sends notification. If
  // was_bookmarked is true, it indicates a bookmark already existed for the
  // URL.
  BookmarkNode* AddNode(BookmarkNode* parent,
                        int index,
                        BookmarkNode* node,
                        bool was_bookmarked);

  // Implementation of GetNodeByID.
  BookmarkNode* GetNodeByID(BookmarkNode* node, int id);

  // Returns true if the parent and index are valid.
  bool IsValidIndex(BookmarkNode* parent, int index, bool allow_end);

  // Sets the date modified time of the specified node.
  void SetDateGroupModified(BookmarkNode* parent, const base::Time time);

  // Creates the bookmark bar/other nodes. These call into
  // CreateRootNodeFromStarredEntry.
  void CreateBookmarkNode();
  void CreateOtherBookmarksNode();

  // Creates a root node (either the bookmark bar node or other node) from the
  // specified starred entry.
  BookmarkNode* CreateRootNodeFromStarredEntry(
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
  void LoadFavIcon(BookmarkNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFavIconLoadRequests(BookmarkNode* node);

  // NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  // Whether the initial set of data has been loaded.
  bool loaded_;

  // The root node. This contains the bookmark bar node and the 'other' node as
  // children.
  BookmarkNode root_;

  BookmarkNode* bookmark_bar_node_;
  BookmarkNode* other_node_;

  // The observers.
  ObserverList<BookmarkModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: nodes_ordered_by_url_set_ is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around url_lock_.
  typedef std::multiset<BookmarkNode*,NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  Lock url_lock_;

  // Used for loading favicons and the empty history request.
  CancelableRequestConsumerTSimple<BookmarkNode*> load_consumer_;

  // Reads/writes bookmarks to disk.
  scoped_refptr<BookmarkStorage> store_;

  // Have we installed a listener on the NotificationService for
  // NOTIFY_HISTORY_LOADED? A listener is installed if the bookmarks file
  // doesn't exist and the history service hasn't finished loading.
  bool waiting_for_history_load_;

  base::WaitableEvent loaded_signal_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_MODEL_H_
