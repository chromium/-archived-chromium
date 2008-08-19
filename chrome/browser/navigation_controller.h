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

#ifndef CHROME_BROWSER_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_NAVIGATION_CONTROLLER_H_

#include <hash_map>

#include "base/ref_counted.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/navigation_controller_base.h"
#include "chrome/browser/session_service.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/ssl_manager.h"

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
// make sure that we have at most one TabContents instance per type
//
////////////////////////////////////////////////////////////////////////////////
class NavigationController : public NavigationControllerBase {
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

  // Overriden to prompt the user if reloading a URL with POST data and the
  // active WebContents isn't showing the POST interstitial page.
  virtual void Reload();

  // Same as Reload, but doesn't check if current entry has POST data.
  void ReloadDontCheckForRepost();

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

  virtual void DidNavigateToEntry(NavigationEntry* entry);
  // Calling this may cause the active tab contents to switch if the current
  // entry corresponds to a different tab contents type.
  virtual void DiscardPendingEntry();

  virtual void InsertEntry(NavigationEntry* entry);

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

 private:
  class RestoreHelper;
  friend class RestoreHelper;

  // For invoking OnReservedPageIDRange.
  friend class TabContents;
  // For invoking GetMaxPageID.
  friend class WebContents;
  // For invoking GetMaxPageID.
  friend class printing::PrintViewManager;

  virtual int GetMaxPageID() const;
  virtual void NavigateToPendingEntry(bool reload);
  virtual void NotifyNavigationEntryCommitted();

  // Lets the history database know navigation entries have been removed.
  virtual void NotifyPrunedEntries();

  // Updates the history database with the active entry and index.
  // Also asks the notifies the active TabContents to notify its
  // delegate about the navigation.
  virtual void IndexOfActiveEntryChanged(int prev_commited_index);

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

  // The user profile associated with this controller
  Profile* profile_;

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

  DISALLOW_COPY_AND_ASSIGN(NavigationController);
};

#endif  // CHROME_BROWSER_NAVIGATION_CONTROLLER_H_
