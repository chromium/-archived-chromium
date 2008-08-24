// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_NAVIGATION_CONTROLLER_H_

#include <hash_map>

#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/session_service.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/ssl_manager.h"
#include "chrome/browser/tab_contents_type.h"

class GURL;
class Profile;
class TabContents;
class WebContents;
class TabContentsCollector;
struct TabNavigation;

namespace printing {
class PrintViewManager;
}

////////////////////////////////////////////////////////////////////////////////
//
// NavigationController class
//
// A NavigationController maintains navigation data. We have one
// NavigationController instance per tab.
//
// The NavigationController also owns all TabContents for the tab. This is to
// make sure that we have at most one TabContents instance per type.
//
////////////////////////////////////////////////////////////////////////////////
class NavigationController {
 public:
  // Provides the details for a NOTIFY_NAV_ENTRY_CHANGED notification.
  struct EntryChangedDetails {
    // The changed navigation entry after it has been updated.
    const NavigationEntry* changed_entry;

    // Indicates the current index in the back/forward list of the entry.
    int index;
  };

  NavigationController(TabContents* initial_contents, Profile* profile);
  // Creates a NavigationController from the specified history. Processing
  // for this is asynchronous and handled via the RestoreHelper (in
  // navigation_controller.cc).
  NavigationController(
      Profile* profile,
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      HWND parent);
  ~NavigationController();

  // Same as Reload, but doesn't check if current entry has POST data.
  void ReloadDontCheckForRepost();

  // Returns the active entry, which is the pending entry if a navigation is in
  // progress or the last committed entry otherwise.  NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationControllerBase,
  // this is the method you will typically want to call.
  //
  NavigationEntry* GetActiveEntry() const;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  int GetCurrentEntryIndex() const;

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  NavigationEntry* GetPendingEntry() const {
    return pending_entry_;
  }

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  int GetPendingEntryIndex() const {
    return pending_entry_index_;
  }

    // Returns the last committed entry, which may be null if there are no
  // committed entries.
  NavigationEntry* GetLastCommittedEntry() const;

  // Returns the index of the last committed entry.
  int GetLastCommittedEntryIndex() const {
    return last_committed_entry_index_;
  }

  // Returns the number of entries in the NavigationControllerBase, excluding
  // the pending entry if there is one.
  int GetEntryCount() const {
    return static_cast<int>(entries_.size());
  }

  NavigationEntry* GetEntryAtIndex(int index) const {
    return entries_.at(index).get();
  }

  // Returns the entry at the specified offset from current.  Returns NULL
  // if out of bounds.
  NavigationEntry* GetEntryAtOffset(int offset) const;

  bool CanStop() const;

  // Return whether this controller can go back.
  bool CanGoBack() const;

  // Return whether this controller can go forward.
  bool CanGoForward() const;

  // Causes the controller to go back.
  void GoBack();

  // Causes the controller to go forward.
  void GoForward();

  // Causes the controller to go to the specified index.
  void GoToIndex(int index);

  // Causes the controller to go to the specified offset from current.  Does
  // nothing if out of bounds.
  void GoToOffset(int offset);

  // Causes the controller to stop a pending navigation if any.
  void Stop();

  // Causes the controller to reload the current entry. Will prompt the user if
  // reloading a URL with POST data and the active WebContents isn't showing the
  // POST interstitial page.
  void Reload();

  // Return the entry with the corresponding type, instance, and page_id, or
  // NULL if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  NavigationEntry* GetEntryWithPageID(TabContentsType type,
                                      SiteInstance* instance,
                                      int32 page_id) const;

  // Causes the controller to load the specified entry.  The controller
  // assumes ownership of the entry.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(NavigationEntry* entry);

  // Ensure the given NavigationEntry has a valid state, so that WebKit does
  // not get confused.
  static void SetContentStateIfEmpty(NavigationEntry* entry);

  // Begin the destruction sequence for this NavigationController and all its
  // registered tabs.  The sequence is as follows:
  // 1. All tabs are asked to Destroy themselves.
  // 2. When each tab is finished Destroying, it will notify the controller.
  // 3. Once all tabs are Destroyed, the NavigationController deletes itself.
  // This ensures that all the TabContents outlive the NavigationController.
  void Destroy();

  // Notifies the controller that a TabContents that it owns has been destroyed.
  // This is part of the NavigationController's Destroy sequence.
  void TabContentsWasDestroyed(TabContentsType type);

  // Returns the currently-active TabContents associated with this controller.
  // You should use GetActiveEntry instead of this in most cases.
  TabContents* active_contents() const {
    return active_contents_;
  }

  // This can never be null.
  Profile* profile() const {
    return profile_;
  }

  // Returns the TabContents cached on this controller for the given type or
  // NULL if there is none.
  TabContents* GetTabContents(TabContentsType type);

  // Causes the controller to load the specified URL.
  void LoadURL(const GURL& url, PageTransition::Type type);

  // Causes the controller to load the specified URL the next time it becomes
  // active.
  void LoadURLLazily(const GURL& url, PageTransition::Type type,
                     const std::wstring& title, SkBitmap* icon);

  // Returns true if this NavigationController is is configured to load a URL
  // lazily. If true, use GetLazyTitle() and GetLazyFavIcon() to discover the
  // titles and favicons. Since no request was made, this is the only info
  // we have about this page. This feature is used by web application clusters.
  bool LoadingURLLazily();
  const std::wstring& GetLazyTitle() const;
  const SkBitmap& GetLazyFavIcon() const;

  void SetAlternateNavURLFetcher(
      AlternateNavURLFetcher* alternate_nav_url_fetcher);

  // --------------------------------------------------------------------------
  // For use by TabContents implementors:

  // Used to inform the NavigationControllerBase of a navigation being committed
  // for a tab.  The controller takes ownership of the entry.  Any entry located
  // forward to the current entry will be deleted.  The new entry becomes the
  // current entry.
  void DidNavigateToEntry(NavigationEntry* entry);

  // Calling this may cause the active tab contents to switch if the current
  // entry corresponds to a different tab contents type.
  void DiscardPendingEntry();

  // Inserts an entry after the current position, removing all entries after it.
  // The new entry will become the active one.
  void InsertEntry(NavigationEntry* entry);

  // Returns the identifier used by session restore.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window we're in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Broadcasts the NOTIFY_NAV_ENTRY_CHANGED notification for the
  // navigation corresponding to the given page. This will keep things in sync
  // like saved session.
  void NotifyEntryChangedByPageID(TabContentsType type,
                                  SiteInstance* instance,
                                  int32 page_id);

  void SetActive(bool is_active);

  // If this NavigationController was restored from history and the selected
  // page has not loaded, it is loaded.
  void LoadIfNecessary();

  // Clone the receiving navigation controller. Only the active tab contents is
  // duplicated. It is created as a child of the provided HWND.
  NavigationController* Clone(HWND hwnd);

  // Returns true if a reload happens when activated (SetActive(true) is
  // invoked). This is true for session/tab restore and cloned tabs.
  bool needs_reload() const { return needs_reload_; }

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  static void DisablePromptOnRepost();

  // Returns the largest restored page ID seen in this navigation controller,
  // if it was restored from a previous session.  (-1 otherwise)
  int max_restored_page_id() const { return max_restored_page_id_; }

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationControllerBase.
  int GetIndexOfEntry(const NavigationEntry* entry) const;

  // Removes the last committed entry.
  void RemoveLastEntry();

 private:
  FRIEND_TEST(NavigationControllerTest, EnforceMaxNavigationCount);

  class RestoreHelper;
  friend class RestoreHelper;

  // For invoking OnReservedPageIDRange.
  friend class TabContents;
  // For invoking GetMaxPageID.
  friend class WebContents;
  // For invoking GetMaxPageID.
  friend class printing::PrintViewManager;

  // Returns the largest page ID seen.  When PageIDs come in larger than
  // this (via DidNavigateToEntry), we know that we've navigated to a new page.
  int GetMaxPageID() const;

  // Actually issues the navigation held in pending_entry.
  void NavigateToPendingEntry(bool reload);

  // Allows the derived class to issue notifications that a load has been
  // committed.
  void NotifyNavigationEntryCommitted();

  // Invoked when entries have been pruned, or removed. For example, if the
  // current entries are [google, digg, yahoo], with the current entry google,
  // and the user types in cnet, then digg and yahoo are pruned.
  void NotifyPrunedEntries();

  // Invoked when the index of the active entry may have changed.
  // The prev_commited_index parameter specifies the previous value
  // of the last commited index before this navigation event happened
  void IndexOfActiveEntryChanged(int prev_commited_index);

  // Returns the TabContents for the |entry|'s type. If the TabContents
  // doesn't yet exist, it is created. If a new TabContents is created, its
  // parent is |parent|.  Becomes part of |entry|'s SiteInstance.
  TabContents* GetTabContentsCreateIfNecessary(HWND parent,
                                               const NavigationEntry& entry);

  // Register the provided tab contents. This tab contents will be owned
  // and deleted by this navigation controller
  void RegisterTabContents(TabContents* some_contents);

  // Broadcasts a notification that the given entry changed.
  void NotifyEntryChanged(const NavigationEntry* entry, int index);

  // Implementation of Reset and the destructor. Deletes entries
  void ResetInternal();

  // Removes the entry at the specified index.  Note that you should not remove
  // the pending entry or the last committed entry.
  void RemoveEntryAtIndex(int index);

  // Sets the max restored page ID this NavigationController has seen, if it
  // was restored from a previous session.
  void set_max_restored_page_id(int max_id) { max_restored_page_id_ = max_id; }

  NavigationEntry* CreateNavigationEntry(const GURL& url,
                                         PageTransition::Type transition);

  // Schedule the TabContents currently allocated for |tc| for collection.
  // The TabContents will be destroyed later from a different event.
  void ScheduleTabContentsCollection(TabContentsType t);

  // Cancel the collection of the TabContents allocated for |tc|. This method
  // is used when we keep using a TabContents because a provisional load failed.
  void CancelTabContentsCollection(TabContentsType t);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents.
  void FinishRestore(HWND parent_hwnd, int selected_index);

  // Discards the pending entry without updating active_contents_
  void DiscardPendingEntryInternal();

  // Return the index of the entry with the corresponding type, instance, and
  // page_id, or -1 if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  int GetEntryIndexWithPageID(TabContentsType type,
                              SiteInstance* instance,
                              int32 page_id) const;

  // The user profile associated with this controller
  Profile* profile_;

  // List of NavigationEntry for this tab
  typedef std::vector<linked_ptr<NavigationEntry> > NavigationEntries;
  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // == -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  NavigationEntry* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

  // Tab contents. One entry per type used. The tab controller owns
  // every tab contents used.
  typedef stdext::hash_map<TabContentsType, TabContents*> TabContentsMap;
  TabContentsMap tab_contents_map_;

  // A map of TabContentsType -> TabContentsCollector containing all the
  // pending collectors.
  typedef stdext::hash_map<TabContentsType, TabContentsCollector*>
  TabContentsCollectorMap;
  TabContentsCollectorMap tab_contents_collector_map_;

  // The tab contents that is currently active.
  TabContents* active_contents_;

  // The AlternateNavURLFetcher and its associated active entry, if any.
  scoped_ptr<AlternateNavURLFetcher> alternate_nav_url_fetcher_;
  int alternate_nav_url_fetcher_entry_unique_id_;

  // The max restored page ID in this controller, if it was restored.  We must
  // store this so that WebContents can tell any renderer in charge of one of
  // the restored entries to update its max page ID.
  int max_restored_page_id_;

  // Manages the SSL security UI
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_;

  // If true, the pending entry is lazy and should be loaded as soon as this
  // controller becomes active.
  bool load_pending_entry_when_active_;

  // Unique identifier of this controller for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  SessionID session_id_;

  // Unique identifier of the window we're in. Used by session restore.
  SessionID window_id_;

  // Should Reload check for post data? The default is true, but is set to false
  // when testing.
  static bool check_for_repost_;

  // The maximum number of entries that a navigation controller can store.
  size_t max_entry_count_;

  DISALLOW_COPY_AND_ASSIGN(NavigationController);
};

#endif  // CHROME_BROWSER_NAVIGATION_CONTROLLER_H_

