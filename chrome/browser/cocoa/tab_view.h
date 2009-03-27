// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_VIEW_H_
#define CHROME_BROWSER_COCOA_TAB_VIEW_H_

#import <Cocoa/Cocoa.h>

@class TabController;

// A view that handles the event tracking (clicking and dragging) for a tab
// on the tab strip. Relies on an associated TabController to provide a
// target/action for selecting the tab.

@interface TabView : NSView {
 @private
  IBOutlet TabController* controller_;
  // TODO(rohitrao): Add this button to a CoreAnimation layer so we can fade it
  // in and out on mouseovers.
  IBOutlet NSButton* closeButton_;
}
@end

#endif  // CHROME_BROWSER_COCOA_TAB_VIEW_H_
