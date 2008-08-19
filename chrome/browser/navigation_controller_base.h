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

#ifndef CHROME_BROWSER_NAVIGATION_CONTROLLER_BASE_H_
#define CHROME_BROWSER_NAVIGATION_CONTROLLER_BASE_H_

#include <vector>

#include "chrome/browser/tab_contents_type.h"
#include "chrome/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class NavigationEntry;
class SiteInstance;

////////////////////////////////////////////////////////////////////////////////
//
// NavigationControllerBase class
//
// A NavigationControllerBase maintains navigation data (like session history).
//
////////////////////////////////////////////////////////////////////////////////
class NavigationControllerBase {
 public:
  NavigationControllerBase();
  virtual ~NavigationControllerBase();

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
    return entries_.at(index);
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

  // Causes the controller to reload the current entry.
  void Reload();

  // Causes the controller to load the specified entry.  The controller
  // assumes ownership of the entry.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(NavigationEntry* entry);

  // Return the entry with the corresponding type, instance, and page_id, or
  // NULL if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  NavigationEntry* GetEntryWithPageID(TabContentsType type,
                                      SiteInstance* instance,
                                      int32 page_id) const;

  // Ensure the given NavigationEntry has a valid state, so that WebKit does
  // not get confused.
  static void SetContentStateIfEmpty(NavigationEntry* entry);

#ifndef NDEBUG
  void Dump();
#endif

  // --------------------------------------------------------------------------
  // For use by NavigationControllerBase clients:

  // Used to inform the NavigationControllerBase of a navigation being committed
  // for a tab.  The controller takes ownership of the entry.  Any entry located
  // forward to the current entry will be deleted.  The new entry becomes the
  // current entry.
  virtual void DidNavigateToEntry(NavigationEntry* entry);

  // Used to inform the NavigationControllerBase to discard its pending entry.
  virtual void DiscardPendingEntry();

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationControllerBase.
  int GetIndexOfEntry(const NavigationEntry* entry) const;

  // Removes the last committed entry.
  void RemoveLastEntry();

 protected:
  // Returns the largest page ID seen.  When PageIDs come in larger than
  // this (via DidNavigateToEntry), we know that we've navigated to a new page.
  virtual int GetMaxPageID() const = 0;

  // Actually issues the navigation held in pending_entry.
  virtual void NavigateToPendingEntry(bool reload) = 0;

  // Allows the derived class to issue notifications that a load has been
  // committed.
  virtual void NotifyNavigationStateChanged() {}

  // Invoked when entries have been pruned, or removed. For example, if the
  // current entries are [google, digg, yahoo], with the current entry google,
  // and the user types in cnet, then digg and yahoo are pruned.
  virtual void NotifyPrunedEntries() {}

  // Invoked when the index of the active entry may have changed.
  // The prev_commited_index parameter specifies the previous value
  // of the last commited index before this navigation event happened
  virtual void IndexOfActiveEntryChanged(int prev_committed_index) {}

  // Inserts an entry after the current position, removing all entries after it.
  // The new entry will become the active one.
  virtual void InsertEntry(NavigationEntry* entry);

  // Discards the pending entry without updating active_contents_
  void DiscardPendingEntryInternal();

  // Return the index of the entry with the corresponding type, instance, and
  // page_id, or -1 if not found.  Use a NULL instance if the type is not
  // TAB_CONTENTS_WEB.
  int GetEntryIndexWithPageID(TabContentsType type,
                              SiteInstance* instance,
                              int32 page_id) const;

  // List of NavigationEntry for this tab
  typedef std::vector<NavigationEntry*> NavigationEntries;
  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  NavigationEntry* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

 private:
  FRIEND_TEST(NavigationControllerTest, EnforceMaxNavigationCount);

  // Implementation of Reset and the destructor. Deletes entries
  void ResetInternal();

  // Removes the entry at the specified index.  Note that you should not remove
  // the pending entry or the last committed entry.
  void RemoveEntryAtIndex(int index);

  // The maximum number of entries that a navigation controller can store.
  size_t max_entry_count_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerBase);
};

#endif  // CHROME_BROWSER_NAVIGATION_CONTROLLER_BASE_H_
