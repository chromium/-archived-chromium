// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCISSIBILITY_BROWSER_IMPL_H__
#define CHROME_TEST_ACCISSIBILITY_BROWSER_IMPL_H__

#include <vector>

#include "tab_impl.h"

/////////////////////////////////////////////////////////////////////
// CBrowserImpl
// It is a wrapper to Browser specific functionalities.
// Note:
// In most of the tasks, keyboard messages are used for now.
// DoDefaultAction() will be called on accessibility objects,
// once implementation is available in chrome accessibility server.
// And keyboard messages will be tested using ApplyAccelerator().
/////////////////////////////////////////////////////////////////////

class CBrowserImpl {
 public:
  CBrowserImpl() {
    active_tab_index_ = 0;
  }

  // Starts Chrome. Sets active tab index.
  bool Launch(void);

  // Closes Chrome.
  bool Quit(void);

  // Activates the specified tab.
  bool ActivateTab(const INT64 index);

  // Returns URL of the active tab.
  bool GetActiveTabURL(BSTR* url);

  // Gets active tab's title.
  bool GetActiveTabTitle(BSTR* title);

  // Gets active tab index.
  bool GetActiveTabIndex(INT64* index);

  // Returns active tab object.
  bool GetActiveTab(CTabImpl** tab);

  // Returns no. of tabs in tabstrip.
  bool GetTabCount(INT64* count);

  bool GetBrowserProcessCount(INT64* count);

  // Reads browser title, which is also a active tab's title
  bool GetBrowserTitle(BSTR* title);

  // Adds new tab. Maintain current active tab index.
  // Returns created tab, if requested.
  bool AddTab(CTabImpl** tab);

  // Returns tab object of specified index.
  bool GetTab(const INT64 index, CTabImpl** tab);

  // Activate tab of specified index. Maintain current active tab index.
  // Returns created tab, if requested.
  bool GoToTab(const INT64 index, CTabImpl** tab);

  // Move to next tab. Maintain current active tab index.
  // Returns created tab, if requested.
  bool GoToNextTab(CTabImpl** tab);

  // Move to previous tab. Maintain current active tab index.
  // Returns created tab, if requested.
  bool GoToPrevTab(CTabImpl** tab);

  // Wait for chrome window to be visible. It checks for accessibility object
  // for tabstrip after every 'interval' for the specified 'timeout'.
  bool WaitForChromeToBeVisible(const INT64 interval, const INT64 timeout,
                                bool* visible);
  bool WaitForTabCountToChange(const INT64 interval, const INT64 timeout,
                               bool* changed);
  bool WaitForTabToBecomeActive(const INT64 index, const INT64 interval,
                                const INT64 timeout, bool* activated);

  // Sends keyboard message. Sends accelerators.
  bool ApplyAccelerator(VARIANT keys);

  // Sets active tab index.
  void SetActiveTabIndex(INT64 index);

  // Removed tab from tab collection vector.
  void CloseTabFromCollection(INT64 index);

  // Updates tab collection vector
  void UpdateTabCollection(void);

  // Removes tab from tab collection vector.
  void EraseTabCollection(void);

 private:
  // Index of active tab.
  INT64 active_tab_index_;

  // Collection of tab data.
  std::vector<ChromeTab*> tab_collection_;
};


#endif  // CHROME_TEST_ACCISSIBILITY_BROWSER_IMPL_H__

