// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class PageInfoWindow;
class PageInfoWindowMac;

// This NSWindowController subclass implements the Cocoa window for
// PageInfoWindow. This creates and owns the PageInfoWindowMac subclass.

@interface PageInfoWindowController : NSWindowController {
 @private
  // We load both images and then we share the refs with our UI elements.
  scoped_nsobject<NSImage> goodImg_;
  scoped_nsobject<NSImage> badImg_;

  // User interface item values. The NIB uses KVO to get these, so the values
  // are not explicitly set in the view by the controller.
  NSImage* identityImg_;
  NSImage* connectionImg_;
  NSImage* historyImg_;
  NSString* identityMsg_;
  NSString* connectionMsg_;
  NSString* historyMsg_;
  BOOL enableCertButton_;

  // Box that allows us to show/hide the history information.
  IBOutlet NSBox* historyBox_;

  // Bridge to Chromium that we own.
  scoped_ptr<PageInfoWindowMac> pageInfo_;
}

@property(readwrite, retain) NSImage* identityImg;
@property(readwrite, retain) NSImage* connectionImg;
@property(readwrite, retain) NSImage* historyImg;
@property(readwrite, copy) NSString* identityMsg;
@property(readwrite, copy) NSString* connectionMsg;
@property(readwrite, copy) NSString* historyMsg;
@property(readwrite) BOOL enableCertButton;

// Returns the bridge between Cocoa and Chromium.
- (PageInfoWindow*)pageInfo;

// Returns the good and bad image refs.
- (NSImage*)goodImg;
- (NSImage*)badImg;

// Shows the certificate display window
- (IBAction)showCertWindow:(id)sender;

// Sets whether or not to show or hide the history box. This will resize the
// frame of the window.
- (void)setShowHistoryBox:(BOOL)show;

@end
