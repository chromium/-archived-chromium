// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_H__
#define CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_H__

#include "chrome/views/menu.h"

class Browser;
class TabContents;
class SkBitmap;
class NavigationEntry;

///////////////////////////////////////////////////////////////////////////////
//
// BackForwardMenuModel
//
// Implements the showing of the dropdown menu for the Back/Forward buttons.
///////////////////////////////////////////////////////////////////////////////
class BackForwardMenuModel : public Menu::Delegate {
 public:
  // These are IDs used to identify individual UI elements within the
  // browser window using View::GetViewByID.
  typedef enum ModelType {
    FORWARD_MENU_DELEGATE = 1,
    BACKWARD_MENU_DELEGATE = 2
  };

  BackForwardMenuModel(Browser* browser, ModelType model_type);
  virtual ~BackForwardMenuModel();

  // Menu::Delegate
  virtual std::wstring GetLabel(int menu_id) const;
  virtual const SkBitmap& GetIcon(int menu_id) const;
  virtual bool SupportsCommand(int menu_id) const;
  virtual bool IsCommandEnabled(int menu_id) const;
  virtual bool IsItemSeparator(int menu_id) const;
  virtual bool HasIcon(int menu_id) const;
  virtual void ExecuteCommand(int menu_id);
  virtual void MenuWillShow();
  // Returns how many items the menu should show, including history items,
  // chapter-stops, separators and the Show Full History link. This function
  // uses GetHistoryItemCount() and GetChapterStopCount() internally to figure
  // out the total number of items to show.
  virtual int GetItemCount() const;

  // Returns how many history items the menu should show. For example, if the
  // navigation controller of the current tab has a current entry index of 5 and
  // forward_direction_ is false (we are the back button delegate) then this
  // function will return 5 (representing 0-4). If forward_direction_ is
  // true (we are the forward button delegate), then this function will return
  // the number of entries after 5. Note, though, that in either case it will
  // not report more than kMaxHistoryItems. The number returned also does not
  // include the separator line after the history items (nor the separator for
  // the "Show Full History" link).
  int GetHistoryItemCount() const;

  // Returns how many chapter-stop items the menu should show. For the
  // definition of a chapter-stop, see GetIndexOfNextChapterStop(). The number
  // returned does not include the separator lines before and after the
  // chapter-stops.
  int GetChapterStopCount(int history_items) const;

  // Finds the next chapter-stop in the NavigationEntryList starting from
  // the index specified in |start_from| and continuing in the direction
  // specified (|forward|) until either a chapter-stop is found or we reach the
  // end, in which case -1 is returned. If |start_from| is out of bounds, -1
  // will also be returned. A chapter-stop is defined as the last page the user
  // browsed to within the same domain. For example, if the user's homepage is
  // Google and she navigates to Google pages G1, G2 and G3 before heading over
  // to WikiPedia for pages W1 and W2 and then back to Google for pages G4 and
  // G5 then G3, W2 and G5 are considered chapter-stops. The return value from
  // this function is an index into the NavigationEntryList vector.
  int GetIndexOfNextChapterStop(int start_from, bool forward) const;

  // Finds a given chapter-stop starting at the currently active entry in the
  // NavigationEntryList vector advancing first forward or backward by |offset|
  // (depending on the direction specified in parameter |forward|). It also
  // allows you to skip chapter-stops by specifying a positive value for |skip|.
  // Example: FindChapterStop(5, false, 3) starts with the currently active
  // index, subtracts 5 from it and then finds the fourth chapter-stop before
  // that index (skipping the first 3 it finds).
  // Example: FindChapterStop(0, true, 0) is functionally equivalent to
  // calling GetIndexOfNextChapterStop(GetCurrentEntryIndex(), true).
  //
  // NOTE: Both |offset| and |skip| must be non-negative. The return value from
  // this function is an index into the NavigationEntryList vector. If |offset|
  // is out of bounds or if we skip too far (run out of chapter-stops) this
  // function returns -1.
  int FindChapterStop(int offset, bool forward, int skip) const;

  // Allows the unit test to use its own dummy tab contents.
  void set_test_tab_contents(TabContents* test_tab_contents) {
    test_tab_contents_ = test_tab_contents;
  }

  // Allow the unit test to use the "Show Full History" label.
  std::wstring GetShowFullHistoryLabel() const;

  // Retrieves the TabContents pointer to use, which is either the one that
  // the unit test sets (using SetTabContentsForUnitTest) or the one from
  // the browser window.
  TabContents* GetTabContents() const;

  // How many items (max) to show in the back/forward history menu dropdown.
  static const int kMaxHistoryItems;

  // How many chapter-stops (max) to show in the back/forward dropdown list.
  static const int kMaxChapterStops;

 private:
  // Converts a menu item id, as passed in through one of the menu delegate
  // functions and converts it into an absolute index into the
  // NavigationEntryList vector. |menu_id| can point to a separator, or the
  // "Show Full History" link in which case this function returns -1.
  int MenuIdToNavEntryIndex(int menu_id) const;

  // Looks up a NavigationEntry by menu id.
  NavigationEntry* GetNavigationEntry(int menu_id) const;

  // Build a string version of a user action on this menu, used as an
  // identifier for logging user behavior.
  // E.g. BuildActionName("Click", 2) returns "BackMenu_Click2".
  // An index of -1 means no index.
  std::wstring BuildActionName(const std::wstring& name, int index) const;

  Browser* browser_;

  // The unit tests will provide their own TabContents to use.
  TabContents* test_tab_contents_;

  // Represents whether this is the delegate for the forward button or the
  // back button.
  ModelType model_type_;

  DISALLOW_EVIL_CONSTRUCTORS(BackForwardMenuModel);
};

#endif  // CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_H__


