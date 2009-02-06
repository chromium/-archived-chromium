// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCATION_BAR_H_
#define CHROME_BROWSER_LOCATION_BAR_H_

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

////////////////////////////////////////////////////////////////////////////////
// LocationBar
//  An interface providing access to the Location Bar component of the browser
//  window.
//
class LocationBar {
 public:
  // Shows the first run information bubble anchored to the location bar.
  virtual void ShowFirstRunBubble() = 0;

  // Returns the string of text entered in the location bar.
  virtual std::wstring GetInputString() const = 0;

  // Returns the WindowOpenDisposition that should be used to determine where to
  // open a URL entered in the location bar.
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;

  // Returns the PageTransition that should be recorded in history when the URL
  // entered in the location bar is loaded.
  virtual PageTransition::Type GetPageTransition() const = 0;

  // Accepts the current string of text entered in the location bar.
  virtual void AcceptInput() = 0;

  // Focuses and selects the contents of the location bar.
  virtual void FocusLocation() = 0;

  // Clears the location bar, inserts an annoying little "?" turd and sets focus
  // to it.
  virtual void FocusSearch() = 0;

  // Saves the state of the location bar to the specified TabContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(TabContents* contents) = 0;
};

#endif  //  #ifndef CHROME_BROWSER_LOCATION_BAR_H_
