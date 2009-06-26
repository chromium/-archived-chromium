// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_

#include "build/build_config.h"

#include <set>
#include <vector>

#include "app/tree_node_model.h"
#include "base/lock.h"
#include "base/observer_list.h"
#include "base/waitable_event.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkEditorView;
class BookmarkIndex;
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
class BookmarkNode : public TreeNode<BookmarkNode> {
  friend class BookmarkModel;

 public:
  // Creates a new node with the specified url and id of 0
  explicit BookmarkNode(const GURL& url);
  // Creates a new node with the specified url and id.
  BookmarkNode(int id, const GURL& url);
  virtual ~BookmarkNode() {}

  // Returns the URL.
  const GURL& GetURL() const { return url_; }

  // Returns a unique id for this node.
  //
  // NOTE: this id is only unique for the session and NOT unique across
  // sessions. Don't persist it!
  int id() const { return id_; }
  // Sets the id to the given value.
  void set_id(int id) { id_ = id; }

  // Returns the type of this node.
  history::StarredEntry::Type GetType() const { return type_; }
  void SetType(history::StarredEntry::Type type) { type_ = type; }

  // Returns the time the bookmark/group was added.
  const base::Time& date_added() const { return date_added_; }
  // Sets the time the bookmark/group was added.
  void set_date_added(const base::Time& date) { date_added_ = date; }

  // Returns the last time the group was modified. This is only maintained
  // for folders (including the bookmark and other folder).
  const base::Time& date_group_modified() const { return date_group_modified_; }
  // Sets the last time the group was modified.
  void set_date_group_modified(const base::Time& date) {
    date_group_modified_ = date;
  }

  // Convenience for testing if this nodes represents a group. A group is
  // a node whose type is not URL.
  bool is_folder() const { return type_ != history::StarredEntry::URL; }

  // Is this a URL?
  bool is_url() const { return type_ == history::StarredEntry::URL; }

  // Returns the favicon. In nearly all cases you should use the method
  // BookmarkModel::GetFavicon rather than this. BookmarkModel::GetFavicon
  // takes care of loading the favicon if it isn't already loaded, where as
  // this does not.
  const SkBitmap& favicon() const { return favicon_; }
  void set_favicon(const SkBitmap& icon) { favicon_ = icon; }

  // The following methods are used by the bookmark model, and are not
  // really useful outside of it.

  bool is_favicon_loaded() const { return loaded_favicon_; }
  void set_favicon_loaded(bool value) { loaded_favicon_ = value; }

  HistoryService::Handle favicon_load_handle() const {
    return favicon_load_handle_;
  }
  void set_favicon_load_handle(HistoryService::Handle handle) {
    favicon_load_handle_ = handle;
  }

  // Called when the favicon becomes invalid.
  void InvalidateFavicon() {
    loaded_favicon_ = false;
    favicon_ = SkBitmap();
  }

  // Resets the properties of the node from the supplied entry.
  // This is used by the bookmark model and not really useful outside of it.
  void Reset(const history::StarredEntry& entry);

  // TODO(sky): Consider adding last visit time here, it'll greatly simplify
  // HistoryContentsProvider.

 private:
  // helper to initialize various fields during construction.
  void Initialize(int id);

  // Unique identifier for this node.
  int id_;

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
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) = 0;

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) = 0;

  // Invoked when a node has been removed, the item may still be starred though.
  // TODO(sky): merge these two into one.
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {
    BookmarkNodeRemoved(model, parent, old_index);
  }

  // Invoked when the title or favicon of a node has changed.
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) = 0;

  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) = 0;

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) = 0;
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
  friend class BookmarkCodecTest;
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
  const BookmarkNode* root_node() { return &root_; }

  // Returns the bookmark bar node. This is NULL until loaded.
  const BookmarkNode* GetBookmarkBarNode() { return bookmark_bar_node_; }

  // Returns the 'other' node. This is NULL until loaded.
  const BookmarkNode* other_node() { return other_node_; }

  // Returns the parent the last node was added to. This never returns NULL
  // (as long as the model is loaded).
  const BookmarkNode* GetParentForNewNodes();

  void AddObserver(BookmarkModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BookmarkModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Unstars or deletes the specified entry. Removing a group entry recursively
  // unstars all nodes. Observers are notified immediately.
  void Remove(const BookmarkNode* parent, int index);

  // Moves the specified entry to a new location.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Returns the favicon for |node|. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const SkBitmap& GetFavIcon(const BookmarkNode* node);

  // Sets the title of the specified node.
  void SetTitle(const BookmarkNode* node, const std::wstring& title);

  // Returns true if the model finished loading.
  bool IsLoaded() { return loaded_; }

  // Returns the set of nodes with the specified URL.
  void GetNodesByURL(const GURL& url, std::vector<const BookmarkNode*>* nodes);

  // Returns the most recently added node for the url. Returns NULL if url is
  // not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedNodeForURL(const GURL& url);

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
  const BookmarkNode* GetNodeByID(int id);

  // Adds a new group node at the specified position.
  const BookmarkNode* AddGroup(const BookmarkNode* parent,
                               int index,
                               const std::wstring& title);

  // Adds a url at the specified position.
  const BookmarkNode* AddURL(const BookmarkNode* parent,
                             int index,
                             const std::wstring& title,
                             const GURL& url);

  // Adds a url with a specific creation date.
  const BookmarkNode* AddURLWithCreationTime(const BookmarkNode* parent,
                                             int index,
                                             const std::wstring& title,
                                             const GURL& url,
                                             const base::Time& creation_time);

  // Sorts the children of |parent|, notifying observers by way of the
  // BookmarkNodeChildrenReordered method.
  void SortChildren(const BookmarkNode* parent);

  // This is the convenience that makes sure the url is starred or not starred.
  // If is_starred is false, all bookmarks for URL are removed. If is_starred is
  // true and there are no bookmarks for url, a bookmark is created.
  void SetURLStarred(const GURL& url,
                     const std::wstring& title,
                     bool is_starred);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created groups from showing up in the
  // combobox of most recently modified groups.
  void ResetDateGroupModified(const BookmarkNode* node);

  void GetBookmarksWithTitlesMatching(
      const std::wstring& text,
      size_t max_count,
      std::vector<bookmark_utils::TitleMatch>* matches);

  Profile* profile() const { return profile_; }

  bool is_root(const BookmarkNode* node) const { return node == &root_; }
  bool is_bookmark_bar_node(const BookmarkNode* node) const {
    return node == bookmark_bar_node_;
  }
  bool is_other_bookmarks_node(const BookmarkNode* node) const {
    return node == other_node_;
  }
  // Returns whether the given node is one of the permanent nodes - root node,
  // bookmark bar node or other bookmarks node.
  bool is_permanent_node(const BookmarkNode* node) const {
    return is_root(node) ||
           is_bookmark_bar_node(node) ||
           is_other_bookmarks_node(node);
  }

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

  // Sets/returns whether or not bookmark IDs are persisted or not.
  bool PersistIDs() const { return persist_ids_; }
  void SetPersistIDs(bool value);

  // Returns whether the bookmarks file changed externally.
  bool file_changed() const { return file_changed_; }

 private:
  // Used to order BookmarkNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Implementation of IsBookmarked. Before calling this the caller must
  // obtain a lock on url_lock_.
  bool IsBookmarkedNoLock(const GURL& url);

  // Overriden to notify the observer the favicon has been loaded.
  void FavIconLoaded(const BookmarkNode* node);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNode(BookmarkNode* node, std::set<GURL>* removed_urls);

  // Invoked when loading is finished. Sets loaded_ and notifies observers.
  // BookmarkModel takes ownership of |details|.
  void DoneLoading(BookmarkStorage::LoadDetails* details);

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
  const BookmarkNode* GetNodeByID(const BookmarkNode* node, int id);

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const BookmarkNode* parent, int index, bool allow_end);

  // Sets the date modified time of the specified node.
  void SetDateGroupModified(const BookmarkNode* parent, const base::Time time);

  // Creates the bookmark bar/other nodes. These call into
  // CreateRootNodeFromStarredEntry.
  BookmarkNode* CreateBookmarkNode();
  BookmarkNode* CreateOtherBookmarksNode();

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

  // Generates and returns the next node ID.
  int generate_next_node_id();

  // Sets the maximum node ID to the given value.
  // This is used by BookmarkCodec to report the maximum ID after it's done
  // decoding since during decoding codec can assign IDs to nodes if IDs are
  // persisted.
  void set_next_node_id(int id) { next_node_id_ = id; }

  // Records that the bookmarks file was changed externally.
  void SetFileChanged();

  // Creates and returns a new LoadDetails. It's up to the caller to delete
  // the returned object.
  BookmarkStorage::LoadDetails* CreateLoadDetails();

  // Registers bookmarks related prefs.
  void RegisterPreferences();
  // Loads bookmark related preferences.
  void LoadPreferences();

  NotificationRegistrar registrar_;

  Profile* profile_;

  // Whether the initial set of data has been loaded.
  bool loaded_;

  // Whether to persist bookmark IDs.
  bool persist_ids_;

  // Whether the bookmarks file was changed externally. This is set after
  // loading is complete and once set the value never changes.
  bool file_changed_;

  // The root node. This contains the bookmark bar node and the 'other' node as
  // children.
  BookmarkNode root_;

  BookmarkNode* bookmark_bar_node_;
  BookmarkNode* other_node_;

  // The maximum ID assigned to the bookmark nodes in the model.
  int next_node_id_;

  // The observers.
  ObserverList<BookmarkModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: nodes_ordered_by_url_set_ is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around url_lock_.
  typedef std::multiset<BookmarkNode*, NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  Lock url_lock_;

  // Used for loading favicons and the empty history request.
  CancelableRequestConsumerTSimple<BookmarkNode*> load_consumer_;

  // Reads/writes bookmarks to disk.
  scoped_refptr<BookmarkStorage> store_;

  scoped_ptr<BookmarkIndex> index_;

  base::WaitableEvent loaded_signal_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
