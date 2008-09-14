// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_NAVIGATION_CONTROLLER_H_

#include <map>

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
struct ViewHostMsg_FrameNavigate_Params;

// A NavigationController maintains the back-forward list for a single tab and
// manages all navigation within that list.
//
// The NavigationController also owns all TabContents for the tab. This is to
// make sure that we have at most one TabContents instance per type.
class NavigationController {
 public:
  // Notification details ------------------------------------------------------

  // Provides the details for a NOTIFY_NAV_ENTRY_CHANGED notification.
  struct EntryChangedDetails {
    // The changed navigation entry after it has been updated.
    const NavigationEntry* changed_entry;

    // Indicates the current index in the back/forward list of the entry.
    int index;
  };

  // Provides the details for a NOTIFY_NAV_ENTRY_COMMITTED notification.
  struct LoadCommittedDetails {
    // By default, the entry will be filled according to a new main frame
    // navigation.
    LoadCommittedDetails()
        : entry(NULL),
          is_auto(false),
          is_in_page(false),
          is_main_frame(true) {
    }

    // The committed entry. This will be the active entry in the controller.
    NavigationEntry* entry;

    // The previous URL that the user was on. This may be empty if none.
    GURL previous_url;

    // True when this load was non-user initated. This corresponds to a
    // a NavigationGestureAuto call from WebKit (see webview_delegate.h).
    // We also count reloads and meta-refreshes as "auto" to account for the
    // fact that WebKit doesn't always set the user gesture properly in these
    // cases (see bug 1051891).
    bool is_auto;

    // True if the navigation was in-page. This means that the active entry's
    // URL and the |previous_url| are the same except for reference fragments.
    bool is_in_page;

    // True when the main frame was navigated. False means the navigation was a
    // sub-frame.
    bool is_main_frame;

    // Returns whether the user probably felt like they navigated somewhere new.
    // We often need this logic for showing or hiding something, and this
    // returns true only for main frame loads that the user initiated, that go
    // to a new page.
    bool is_user_initiated_main_frame_load() const {
      return !is_auto && !is_in_page && is_main_frame;
    }
  };

  // ---------------------------------------------------------------------------

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

  // Begin the destruction sequence for this NavigationController and all its
  // registered tabs.  The sequence is as follows:
  // 1. All tabs are asked to Destroy themselves.
  // 2. When each tab is finished Destroying, it will notify the controller.
  // 3. Once all tabs are Destroyed, the NavigationController deletes itself.
  // This ensures that all the TabContents outlive the NavigationController.
  void Destroy();

  // Clone the receiving navigation controller. Only the active tab contents is
  // duplicated. It is created as a child of the provided HWND.
  NavigationController* Clone(HWND hwnd);

  // Returns the profile for this controller. It can never be NULL.
  Profile* profile() const {
    return profile_;
  }

  // Active entry --------------------------------------------------------------

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

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  NavigationEntry* GetLastCommittedEntry() const;

  // Returns the index of the last committed entry.
  int GetLastCommittedEntryIndex() const {
    return last_committed_entry_index_;
  }

  // Navigation list -----------------------------------------------------------

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

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationControllerBase.
  int GetIndexOfEntry(const NavigationEntry* entry) const;

  // Return the index of the entry with the corresponding type, instance, and
  // page_id, or -1 if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  int GetEntryIndexWithPageID(TabContentsType type,
                              SiteInstance* instance,
                              int32 page_id) const;

  // Return the entry with the corresponding type, instance, and page_id, or
  // NULL if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  NavigationEntry* GetEntryWithPageID(TabContentsType type,
                                      SiteInstance* instance,
                                      int32 page_id) const;

  // Pending entry -------------------------------------------------------------

  // Commits the current pending entry and issues the NOTIFY_NAV_ENTRY_COMMIT
  // notification. No changes are made to the entry during this process, it is
  // just moved from pending to committed. This is an alternative to
  // RendererDidNavigate for simple TabContents types.
  //
  // When the pending entry is a new navigation, it will have a page ID of -1.
  // The caller should leave this as-is. CommitPendingEntry will generate a
  // new page ID for you and update the TabContents with that ID.
  void CommitPendingEntry();

  // Calling this may cause the active tab contents to switch if the current
  // entry corresponds to a different tab contents type.
  void DiscardPendingEntry();

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

  // New navigations -----------------------------------------------------------

  // Loads the specified URL.
  void LoadURL(const GURL& url, PageTransition::Type type);

  // Load the specified URL the next time it becomes active.
  void LoadURLLazily(const GURL& url, PageTransition::Type type,
                     const std::wstring& title, SkBitmap* icon);

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet.
  void LoadIfNecessary();

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  bool CanGoBack() const;
  bool CanGoForward() const;
  void GoBack();
  void GoForward();

  // Navigates to the specified absolute index.
  void GoToIndex(int index);

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  void GoToOffset(int offset);

  // Reloads the current entry. The user will be prompted if the URL has POST
  // data and the active WebContents isn't showing the POST interstitial page.
  void Reload();

  // Same as Reload, but doesn't check if current entry has POST data.
  void ReloadDontCheckForRepost();

  // TabContents ---------------------------------------------------------------

  // Notifies the controller that a TabContents that it owns has been destroyed.
  // This is part of the NavigationController's Destroy sequence.
  void TabContentsWasDestroyed(TabContentsType type);

  // Returns the TabContents cached on this controller for the given type or
  // NULL if there is none.
  TabContents* GetTabContents(TabContentsType type);

  // Returns the currently-active TabContents associated with this controller.
  // You should use GetActiveEntry instead of this in most cases.
  TabContents* active_contents() const {
    return active_contents_;
  }

  // For use by TabContents ----------------------------------------------------

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the WebContents. Simpler tab contents types can use
  // CommitPendingEntry below.
  //
  // If a new entry is created, it will return true and will have filled the
  // given details structure and broadcast the NOTIFY_NAV_ENTRY_COMMITTED
  // notification. The caller can then use the details without worrying about
  // listening for the notification.
  //
  // In the case that nothing has changed, the details structure is undefined
  // and it will return false.
  bool RendererDidNavigate(const ViewHostMsg_FrameNavigate_Params& params,
                           bool is_interstitial,
                           LoadCommittedDetails* details);

  // Inserts a new entry by making a copy of the given navigation entry. This is
  // used by interstitials to create dummy entries that they will be in charge
  // of removing later.
  void AddDummyEntryForInterstitial(const NavigationEntry& clone_me);

  // Removes the last entry in the list. This is used by the interstitial code
  // to delete the dummy entry created by AddDummyEntryForInterstitial. If the
  // last entry is the currently committed one, a ENTRY_COMMITTED notification
  // will be broadcast.
  void RemoveLastEntryForInterstitial();

  // Notifies us that we just became active. This is used by the TabContents
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

  // Broadcasts the NOTIFY_NAV_ENTRY_CHANGED notification for the given entry
  // (which must be at the given index). This will keep things in sync like
  // the saved session.
  void NotifyEntryChanged(const NavigationEntry* entry, int index);

  // Returns true if the given URL would be an in-page navigation (i.e. only
  // the reference fragment is different) from the "last committed entry". We do
  // not compare it against the "active entry" since the active entry can be
  // pending and in page navigations only happen on committed pages. If there
  // is no last committed entry, then nothing will be in-page.
  //
  // Special note: if the URLs are the same, it does NOT count as an in-page
  // navigation. Neither does an input URL that has no ref, even if the rest is
  // the same. This may seem weird, but when we're considering whether a
  // navigation happened without loading anything, the same URL would be a
  // reload, while only a different ref would be in-page (pages can't clear
  // refs without reload, only change to "#" which we don't count as empty).
  bool IsURLInPageNavigation(const GURL& url) const;

  // Random data ---------------------------------------------------------------

  // Returns true if this NavigationController is is configured to load a URL
  // lazily. If true, use GetLazyTitle() and GetLazyFavIcon() to discover the
  // titles and favicons. Since no request was made, this is the only info
  // we have about this page. This feature is used by web application clusters.
  bool LoadingURLLazily();
  const std::wstring& GetLazyTitle() const;
  const SkBitmap& GetLazyFavIcon() const;

  // TODO(brettw) bug 1324500: move this out of here.
  void SetAlternateNavURLFetcher(
      AlternateNavURLFetcher* alternate_nav_url_fetcher);

  // Returns the identifier used by session restore.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window we're in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Returns true if a reload happens when activated (SetActive(true) is
  // invoked). This is true for session/tab restore and cloned tabs.
  bool needs_reload() const { return needs_reload_; }

  // Returns the largest restored page ID seen in this navigation controller,
  // if it was restored from a previous session.  (-1 otherwise)
  int max_restored_page_id() const { return max_restored_page_id_; }

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  static void DisablePromptOnRepost();

 private:
  FRIEND_TEST(NavigationControllerTest, EnforceMaxNavigationCount);
  class RestoreHelper;
  friend class RestoreHelper;
  friend class TabContents;  // For invoking OnReservedPageIDRange.

  // Indicates different types of navigations that can occur that we will handle
  // separately. This is computed by ClassifyNavigation.
  enum NavClass {
    // A new page was navigated in the main frame.
    NAV_NEW_PAGE,

    // Renavigating to an existing navigation entry. The entry is guaranteed to
    // exist in the list, or else it would be a new page or IGNORE navigation.
    NAV_EXISTING_PAGE,

    // The same page has been reloaded as a result of the user requesting
    // navigation to that same page (like pressing Enter in the URL bar). This
    // is not the same as an in-page navigation because we'll actually have a
    // pending entry for the load, which is then meaningless.
    NAV_SAME_PAGE,

    // In page navigations are when the reference fragment changes. This will
    // be in the main frame only (we won't even get notified of in-page
    // subframe navigations). It may be for any page, not necessarily the last
    // committed one (for example, whey going back to a page with a ref).
    NAV_IN_PAGE,

    // A new subframe was manually navigated by the user. We will create a new
    // NavigationEntry so they can go back to the previous subframe content
    // using the back button.
    NAV_NEW_SUBFRAME,

    // A subframe in the page was automatically loaded or navigated to such that
    // a new navigation entry should not be created. There are two cases:
    //  1. Stuff like iframes containing ads that the page loads automatically.
    //     The user doesn't want to see these, so we just update the existing
    //     navigation entry.
    //  2. Going back/forward to previous subframe navigations. We don't create
    //     a new entry here either, just update the last committed entry.
    // These two cases are actually pretty different, they just happen to
    // require almost the same code to handle.
    NAV_AUTO_SUBFRAME,

    // Nothing happened. This happens when we get information about a page we
    // don't know anything about. It can also happen when an iframe in a popup
    // navigated to about:blank is navigated. Nothing needs to be done.
    NAV_IGNORE,
  };

  // Classifies the given renderer navigation (see the NavigationType enum).
  NavClass ClassifyNavigation(
      const ViewHostMsg_FrameNavigate_Params& params) const;

  // Causes the controller to load the specified entry. The function assumes
  // ownership of the pointer since it is put in the navigation list.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(NavigationEntry* entry);

  // Handlers for the different types of navigation types. They will actually
  // handle the navigations corresponding to the different NavClasses above.
  // They will NOT broadcast the commit notification, that should be handled by
  // the caller.
  //
  // RendererDidNavigateAutoSubframe is special, it may not actually change
  // anything if some random subframe is loaded. It will return true if anything
  // changed, or false if not.
  void RendererDidNavigateToNewPage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateToExistingPage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateToSamePage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateInPage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateNewSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);
  bool RendererDidNavigateAutoSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Actually issues the navigation held in pending_entry.
  void NavigateToPendingEntry(bool reload);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(LoadCommittedDetails* details);

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

  // Removes the entry at the specified index.  Note that you should not remove
  // the pending entry or the last committed entry.
  void RemoveEntryAtIndex(int index);

  // Sets the max restored page ID this NavigationController has seen, if it
  // was restored from a previous session.
  void set_max_restored_page_id(int max_id) { max_restored_page_id_ = max_id; }

  NavigationEntry* CreateNavigationEntry(const GURL& url,
                                         PageTransition::Type transition);

  // Invokes ScheduleTabContentsCollection for all TabContents but the active
  // one.
  void ScheduleTabContentsCollectionForInactiveTabs();

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

  // Inserts an entry after the current position, removing all entries after it.
  // The new entry will become the active one.
  void InsertEntry(NavigationEntry* entry);

  // Discards the pending entry without updating active_contents_
  void DiscardPendingEntryInternal();

  // ---------------------------------------------------------------------------

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
  typedef std::map<TabContentsType, TabContents*> TabContentsMap;
  TabContentsMap tab_contents_map_;

  // A map of TabContentsType -> TabContentsCollector containing all the
  // pending collectors.
  typedef base::hash_map<TabContentsType, TabContentsCollector*>
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
