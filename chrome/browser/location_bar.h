// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.

#ifndef CHROME_BROWSER_LOCATION_BAR_H_
#define CHROME_BROWSER_LOCATION_BAR_H_

#include <string>

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class TabContents;

class LocationBar {
 public:
  // Shows the first run information bubble anchored to the location bar.
  virtual void ShowFirstRunBubble() = 0;

  // Returns the string of text entered in the location bar.
  virtual std::wstring GetInputString() const = 0;

  // Returns the WindowOpenDisposition that should be used to determine where
  // to open a URL entered in the location bar.
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;

  // Returns the PageTransition that should be recorded in history when the URL
  // entered in the location bar is loaded.
  virtual PageTransition::Type GetPageTransition() const = 0;

  // Accepts the current string of text entered in the location bar.
  virtual void AcceptInput() = 0;

  // Accept the current input, overriding the disposition.
  virtual void AcceptInputWithDisposition(WindowOpenDisposition) = 0;

  // Focuses and selects the contents of the location bar.
  virtual void FocusLocation() = 0;

  // Clears the location bar, inserts an annoying little "?" turd and sets
  // focus to it.
  virtual void FocusSearch() = 0;

  // Update the state of the feed icon.
  virtual void UpdateFeedIcon() = 0;

  // Saves the state of the location bar to the specified TabContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(TabContents* contents) = 0;
};

#endif  // CHROME_BROWSER_LOCATION_BAR_H_
