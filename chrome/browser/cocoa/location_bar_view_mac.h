// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/location_bar.h"

// A C++ bridge class that handles responding to requests from the
// cross-platform code for information about the location bar.

class LocationBarViewMac : public LocationBar {
 public:
  LocationBarViewMac(NSTextField* field);
  virtual ~LocationBarViewMac();

  // TODO(shess): This is a placeholder for the Omnibox code.  The
  // problem it will paper over is that Profile availability does not
  // match object creation in TabContentsController.  Circle back and
  // resolve this after the Profile-handling and tab logic changes are
  // complete.
  void Init();

  // Overridden from LocationBar
  virtual void ShowFirstRunBubble() { NOTIMPLEMENTED(); }
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const
      { NOTIMPLEMENTED(); return CURRENT_TAB; }
  // TODO(rohitrao): Fix this to return different types once autocomplete and
  // the onmibar are implemented.  For now, any URL that comes from the
  // LocationBar has to have been entered by the user, and thus is of type
  // PageTransition::TYPED.
  virtual PageTransition::Type GetPageTransition() const
      { NOTIMPLEMENTED(); return PageTransition::TYPED; }
  virtual void AcceptInput() { NOTIMPLEMENTED(); }
  virtual void AcceptInputWithDisposition(WindowOpenDisposition disposition)
      { NOTIMPLEMENTED(); }
  virtual void FocusLocation();
  virtual void FocusSearch() { NOTIMPLEMENTED(); }
  virtual void UpdateFeedIcon() { /* http://crbug.com/8832 */ }
  virtual void SaveStateToContents(TabContents* contents) { NOTIMPLEMENTED(); }

 private:
  NSTextField* field_;  // weak, owned by TabContentsController

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
