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
