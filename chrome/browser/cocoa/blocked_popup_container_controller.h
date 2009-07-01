// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/blocked_popup_container.h"

// Controller for the blocked popup view. Communicates with the cross-platform
// code via a C++ bridge class, below. The BlockedPopupContainer class doesn't
// really "own" the bridge, it just keeps a pointer to it and calls Destroy() on
// it when it's supposed to go away. As a result, this class needs to own itself
// (always keep an extra retain), and will autorelease itself (and the bridge
// which it owns) when the bridge gets a Destroy() message.
// TODO(pinkerton): Reverse the ownership if it makes more sense. I'm leaving
// it this way because I assume we eventually want this to be a
// NSViewController, and we usually have the Obj-C controller owning the
// bridge (rather than the other way around).
@interface BlockedPopupContainerController : NSObject {
 @private
  scoped_ptr<BlockedPopupContainerView> bridge_;
  BlockedPopupContainer* container_;  // Weak. "owns" me.
  scoped_nsobject<NSView> view_;
  IBOutlet NSTextField* label_;
}

// Initialize with the given popup container. Creates the C++ bridge object
// used to represet the "view".
- (id)initWithContainer:(BlockedPopupContainer*)container;

// Returns the C++ brige object.
- (BlockedPopupContainerView*)bridge;

// Called by the bridge to perform certain actions from the back-end code.
- (void)show;
- (void)hide;
- (void)update;

@end

@interface BlockedPopupContainerController(ForTesting)
- (NSView*)view;
- (NSView*)label;
- (IBAction)closePopup:(id)sender;
@end

#endif  // CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_CONTROLLER_H_
