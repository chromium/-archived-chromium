// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_

#include <map>

#include "build/build_config.h"

#include "base/linked_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/page_transition_types.h"

class NavigationEntry;
class GURL;
class Profile;
class TabContents;
class SiteInstance;
class SkBitmap;
class TabContents;
class TabContentsCollector;
class TabNavigation;
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
  // TODO(brettw) this mostly duplicates ProvisionalLoadDetails, it would be
  // nice to unify these somehow.
  struct LoadCommittedDetails {
    // By default, the entry will be filled according to a new main frame
    // navigation.
    LoadCommittedDetails()
        : entry(NULL),
          is_auto(false),
          did_replace_entry(false),
          is_in_page(false),
          is_main_frame(true) {
    }

    // The committed entry. This will be the active entry in the controller.
    NavigationEntry* entry;

    // The type of navigation that just occurred. Note that not all types of
    // navigations in the enum are valid here, since some of them don't actually
    // cause a "commit" and won't generate this notification.
    NavigationType::Type type;

    // The index of the previously committed navigation entry. This will be -1
    // if there are no previous entries.
    int previous_entry_index;

    // The previous URL that the user was on. This may be empty if none.
    GURL previous_url;

    // True when this load was non-user initated. This corresponds to a
    // a NavigationGestureAuto call from WebKit (see webview_delegate.h).
    // We also count reloads and meta-refreshes as "auto" to account for the
    // fact that WebKit doesn't always set the user gesture properly in these
    // cases (see bug 1051891).
    bool is_auto;

    // True if the committed entry has replaced the exisiting one.
    // A non-user initiated redierct causes such replacement.
    // This is somewhat similiar to is_auto, but not exactly the same.
    bool did_replace_entry;

    // True if the navigation was in-page. This means that the active entry's
    // URL and the |previous_url| are the same except for reference fragments.
    bool is_in_page;

    // True when the main frame was navigated. False means the navigation was a
    // sub-frame.
    bool is_main_frame;

    // Whether the content of this frame has been altered/blocked because it was
    // unsafe.
    bool is_content_filtered;

    // When the committed load is a web page from the renderer, this string
    // specifies the security state if the page is secure.
    // See ViewHostMsg_FrameNavigate_Params.security_info, where it comes from.
    // Use SSLManager::DeserializeSecurityInfo to decode it.
    std::string serialized_security_info;

    // Returns whether the user probably felt like they navigated somewhere new.
    // We often need this logic for showing or hiding something, and this
    // returns true only for main frame loads that the user initiated, that go
    // to a new page.
    bool is_user_initiated_main_frame_load() const {
      return !is_auto && !is_in_page && is_main_frame;
    }

    // The HTTP status code for this entry..
    int http_status_code;
  };

  // Details sent for NOTIFY_NAV_LIST_PRUNED.
  struct PrunedDetails {
    // If true, count items were removed from the front of the list, otherwise
    // count items were removed from the back of the list.
    bool from_front;

    // Number of items removed.
    int count;
  };

  // ---------------------------------------------------------------------------

  NavigationController(TabContents* tab_contents, Profile* profile);
  ~NavigationController();

  // Returns the profile for this controller. It can never be NULL.
  Profile* profile() const {
    return profile_;
  }

  // Initializes this NavigationController with the given saved navigations,
  // using selected_navigation as the currently loaded entry. Before this call
  // the controller should be unused (there should be no current entry). This is
  // used for session restore.
  void RestoreFromState(const std::vector<TabNavigation>& navigations,
                        int selected_navigation);

  // Active entry --------------------------------------------------------------

  // Returns the active entry, which is the transient entry if any, the pending
  // entry if a navigation is in progress or the last committed entry otherwise.
  // NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationController,
  // this is the method you will typically want to call.
  NavigationEntry* GetActiveEntry() const;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  int GetCurrentEntryIndex() const;

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  NavigationEntry* GetLastCommittedEntry() const;

  // Returns the index of the last committed entry.
  int last_committed_entry_index() const {
    return last_committed_entry_index_;
  }

  // Navigation list -----------------------------------------------------------

  // Returns the number of entries in the NavigationController, excluding
  // the pending entry if there is one, but including the transient entry if
  // any.
  int entry_count() const {
    return static_cast<int>(entries_.size());
  }

  NavigationEntry* GetEntryAtIndex(int index) const {
    return entries_.at(index).get();
  }

  // Returns the entry at the specified offset from current.  Returns NULL
  // if out of bounds.
  NavigationEntry* GetEntryAtOffset(int offset) const;

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const NavigationEntry* entry) const;

  // Return the index of the entry with the corresponding instance and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(SiteInstance* instance,
                              int32 page_id) const;

  // Return the entry with the corresponding instance and page_id, or NULL if
  // not found.
  NavigationEntry* GetEntryWithPageID(SiteInstance* instance,
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

  // Discards the pending and transient entries if any.
  void DiscardNonCommittedEntries();

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  NavigationEntry* pending_entry() const {
    return pending_entry_;
  }

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  int pending_entry_index() const {
    return pending_entry_index_;
  }

  // Transient entry -----------------------------------------------------------

  // Adds an entry that is returned by GetActiveEntry().  The entry is
  // transient: any navigation causes it to be removed and discarded.
  // The NavigationController becomes the owner of |entry| and deletes it when
  // it discards it.  This is useful with interstitial page that need to be
  // represented as an entry, but should go away when the user navigates away
  // from them.
  // Note that adding a transient entry does not change the active contents.
  void AddTransientEntry(NavigationEntry* entry);

  // Returns the transient entry if any.  Note that the returned entry is owned
  // by the navigation controller and may be deleted at any time.
  NavigationEntry* GetTransientEntry() const;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL.
  void LoadURL(const GURL& url, const GURL& referrer,
               PageTransition::Type type);

  // Load the specified URL the next time it becomes active.
  void LoadURLLazily(const GURL& url, const GURL& referrer,
                     PageTransition::Type type, const std::wstring& title,
                     SkBitmap* icon);

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

  // Reloads the current entry. If |check_for_repost| is true and the current
  // entry has POST data the user is prompted to see if they really want to
  // reload the page. In nearly all cases pass in true.
  void Reload(bool check_for_repost);

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  This call dicards any pending
  // and transient entries.  |default_url| is the URL that the navigation
  // controller navigates to if there are no more entries after the removal.
  // If |default_url| is empty, we default to "about:blank".
  void RemoveEntryAtIndex(int index, const GURL& default_url);

  // TabContents ---------------------------------------------------------------

  // Returns the tab contents associated with this controller. Non-NULL except
  // during set-up of the tab.
  TabContents* tab_contents() const {
    // This currently returns the active tab contents which should be renamed to
    // tab_contents.
    return tab_contents_;
  }

  // Called when a document has been loaded in a frame.
  void DocumentLoadedInFrame();

  // Called when the user presses the mouse, enter key or space bar.
  void OnUserGesture();

  // For use by TabContents ----------------------------------------------------

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the TabContents. Simpler tab contents types can use
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
                           LoadCommittedDetails* details);

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

  // Copies the navigation state from the given controller to this one. This
  // one should be empty (just created).
  void CopyStateFrom(const NavigationController& source);

  // Random data ---------------------------------------------------------------

  // Returns true if this NavigationController is is configured to load a URL
  // lazily. If true, use GetLazyTitle() and GetLazyFavIcon() to discover the
  // titles and favicons. Since no request was made, this is the only info
  // we have about this page. This feature is used by web application clusters.
  bool LoadingURLLazily() const;
  const string16& GetLazyTitle() const;
  const SkBitmap& GetLazyFavIcon() const;

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

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count(size_t max_entry_count) {
    max_entry_count_ = max_entry_count;
  }
  static size_t max_entry_count() { return max_entry_count_; }

 private:
  class RestoreHelper;
  friend class RestoreHelper;
  friend class TabContents;  // For invoking OnReservedPageIDRange.

  // Classifies the given renderer navigation (see the NavigationType enum).
  NavigationType::Type ClassifyNavigation(
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
  //
  // The functions taking |did_replace_entry| will fill into the given variable
  // whether the last entry has been replaced or not.
  // See LoadCommittedDetails.did_replace_entry.
  void RendererDidNavigateToNewPage(
      const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry);
  void RendererDidNavigateToExistingPage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateToSamePage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateInPage(
      const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry);
  void RendererDidNavigateNewSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);
  bool RendererDidNavigateAutoSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Actually issues the navigation held in pending_entry.
  void NavigateToPendingEntry(bool reload);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(LoadCommittedDetails* details);

  // Sets the max restored page ID this NavigationController has seen, if it
  // was restored from a previous session.
  void set_max_restored_page_id(int max_id) { max_restored_page_id_ = max_id; }

  NavigationEntry* CreateNavigationEntry(const GURL& url, const GURL& referrer,
                                         PageTransition::Type transition);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents.
  void FinishRestore(int selected_index);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  void InsertOrReplaceEntry(NavigationEntry* entry, bool replace);

  // Discards the pending and transient entries.
  void DiscardNonCommittedEntriesInternal();

  // Discards the transient entry.
  void DiscardTransientEntry();

  // Returns true if the navigation is redirect.
  bool IsRedirect(const ViewHostMsg_FrameNavigate_Params& params);

  // Returns true if the navigation is likley to be automatic rather than
  // user-initiated.
  bool IsLikelyAutoNavigation(base::TimeTicks now);

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

  // The index for the entry that is shown until a navigation occurs.  This is
  // used for interstitial pages. -1 if there are no such entry.
  // Note that this entry really appears in the list of entries, but only
  // temporarily (until the next navigation).  Any index poiting to an entry
  // after the transient entry will become invalid if you navigate forward.
  int transient_entry_index_;

  // The tab contents associated with the controller. Possibly NULL during
  // setup.
  TabContents* tab_contents_;

  // The max restored page ID in this controller, if it was restored.  We must
  // store this so that TabContents can tell any renderer in charge of one of
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
  const SessionID session_id_;

  // Unique identifier of the window we're in. Used by session restore.
  SessionID window_id_;

  // The time ticks at which the last document was loaded.
  base::TimeTicks last_document_loaded_;

  // Whether a user gesture has been observed since the last navigation.
  bool user_gesture_observed_;

  // Should Reload check for post data? The default is true, but is set to false
  // when testing.
  static bool check_for_repost_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_;

  DISALLOW_COPY_AND_ASSIGN(NavigationController);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_
