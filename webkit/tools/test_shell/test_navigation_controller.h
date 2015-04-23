// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebDataSource.h"

class GURL;
class TestShell;

// Associated with browser-initated navigations to hold tracking data.
class TestShellExtraData : public WebKit::WebDataSource::ExtraData {
 public:
  TestShellExtraData(int32 pending_page_id)
      : pending_page_id(pending_page_id),
        request_committed(false) {
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id;

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed;
};

// Stores one back/forward navigation state for the test shell.
class TestNavigationEntry {
 public:
  TestNavigationEntry();
  TestNavigationEntry(int page_id,
                      const GURL& url,
                      const std::wstring& title,
                      const std::wstring& target_frame);

  // Virtual to allow test_shell to extend the class.
  ~TestNavigationEntry();

  // Set / Get the URI
  void SetURL(const GURL& url) { url_ = url; }
  const GURL& GetURL() const { return url_; }

  // Set / Get the title
  void SetTitle(const std::wstring& a_title) { title_ = a_title; }
  const std::wstring& GetTitle() const { return title_; }

  // Set / Get opaque state.
  // WARNING: This state is saved to the database and used to restore previous
  // states. If you use write a custom TabContents and provide your own
  // state make sure you have the ability to modify the format in the future
  // while being able to deal with older versions.
  void SetContentState(const std::string& state);
  const std::string& GetContentState() const { return state_; }

  // Get the page id corresponding to the tab's state.
  void SetPageID(int page_id) { page_id_ = page_id; }
  int32 GetPageID() const { return page_id_; }

  const std::wstring& GetTargetFrame() const { return target_frame_; }

 private:
  // Describes the current page that the tab represents. This is not relevant
  // for all tab contents types.
  int32 page_id_;

  GURL url_;
  std::wstring title_;
  std::string state_;

  std::wstring target_frame_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationEntry);
};

// Test shell's NavigationController.  The goal is to be as close to the Chrome
// version as possible.
class TestNavigationController {
 public:
  TestNavigationController(TestShell* shell);
  ~TestNavigationController();

  void Reset();

  // Causes the controller to reload the current (or pending) entry.
  void Reload();

  // Causes the controller to go to the specified offset from current.  Does
  // nothing if out of bounds.
  void GoToOffset(int offset);

  // Causes the controller to go to the specified index.
  void GoToIndex(int index);

  // Causes the controller to load the specified entry.  The controller
  // assumes ownership of the entry.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(TestNavigationEntry* entry);

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  TestNavigationEntry* GetLastCommittedEntry() const;

  // Returns the number of entries in the NavigationControllerBase, excluding
  // the pending entry if there is one.
  int GetEntryCount() const {
    return static_cast<int>(entries_.size());
  }

  // Returns the active entry, which is the pending entry if a navigation is in
  // progress or the last committed entry otherwise.  NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationControllerBase,
  // this is the method you will typically want to call.
  TestNavigationEntry* GetActiveEntry() const;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  int GetCurrentEntryIndex() const;

  // Returns the entry at the specified index.  Returns NULL if out of
  // bounds.
  TestNavigationEntry* GetEntryAtIndex(int index) const;

  // Return the entry with the corresponding type and page_id, or NULL if
  // not found.
  TestNavigationEntry* GetEntryWithPageID(int32 page_id) const;

  // Returns the index of the last committed entry.
  int GetLastCommittedEntryIndex() const {
    return last_committed_entry_index_;
  }

  // Used to inform us of a navigation being committed for a tab. We will take
  // ownership of the entry. Any entry located forward to the current entry will
  // be deleted. The new entry becomes the current entry.
  void DidNavigateToEntry(TestNavigationEntry* entry);

  // Used to inform us to discard its pending entry.
  void DiscardPendingEntry();

 private:
  // Inserts an entry after the current position, removing all entries after it.
  // The new entry will become the active one.
  void InsertEntry(TestNavigationEntry* entry);

  int GetMaxPageID() const { return max_page_id_; }
  void NavigateToPendingEntry(bool reload);

  // Return the index of the entry with the corresponding type and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(int32 page_id) const;

  // Updates the max page ID with that of the given entry, if is larger.
  void UpdateMaxPageID();

  // List of NavigationEntry for this tab
  typedef std::vector< linked_ptr<TestNavigationEntry> > NavigationEntryList;
  typedef NavigationEntryList::iterator NavigationEntryListIterator;
  NavigationEntryList entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  TestNavigationEntry* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

  TestShell* shell_;
  int max_page_id_;

  DISALLOW_EVIL_CONSTRUCTORS(TestNavigationController);
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H_
