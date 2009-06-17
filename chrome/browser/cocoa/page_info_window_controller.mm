// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/page_info_window_controller.h"

#include "base/mac_util.h"
#include "chrome/browser/cocoa/page_info_window_mac.h"

@implementation PageInfoWindowController
@synthesize identityImg = identityImg_;
@synthesize connectionImg = connectionImg_;
@synthesize historyImg = historyImg_;
@synthesize identityMsg = identityMsg_;
@synthesize connectionMsg = connectionMsg_;
@synthesize historyMsg = historyMsg_;
@synthesize enableCertButton = enableCertButton_;

- (id)init {
  NSBundle* bundle = mac_util::MainAppBundle();
  NSString* nibpath = [bundle pathForResource:@"PageInfo" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    pageInfo_.reset(new PageInfoWindowMac(self));

    // Load the image refs.
    NSImage* img = [[NSImage alloc] initByReferencingFile:
                    [bundle pathForResource:@"pageinfo_good" ofType:@"png"]];
    goodImg_.reset(img);

    img = [[NSImage alloc] initByReferencingFile:
           [bundle pathForResource:@"pageinfo_bad" ofType:@"png"]];
    badImg_.reset(img);
  }
  return self;
}

- (void)awakeFromNib {
  // By default, assume we have no history information.
  [self setShowHistoryBox:NO];
}

- (void)dealloc {
  [identityImg_ release];
  [connectionImg_ release];
  [historyImg_ release];
  [identityMsg_ release];
  [connectionMsg_ release];
  [historyMsg_ release];
  [super dealloc];
}

- (PageInfoWindow*)pageInfo {
  return pageInfo_.get();
}

- (NSImage*)goodImg {
  return goodImg_.get();
}

- (NSImage*)badImg {
  return badImg_.get();
}

- (IBAction)showCertWindow:(id)sender {
  pageInfo_->ShowCertDialog(0);  // Pass it any int because it's ignored.
}

- (void)setShowHistoryBox:(BOOL)show {
  [historyBox_ setHidden:!show];

  NSWindow* window = [self window];
  NSRect frame = [window frame];

  const NSSize kPageInfoWindowSize = NSMakeSize(460, 235);
  const NSSize kPageInfoWindowWithHistorySize = NSMakeSize(460, 310);

  frame.size = (show ? kPageInfoWindowWithHistorySize : kPageInfoWindowSize);

  [window setFrame:frame display:YES animate:YES];
}

// If the page info window gets closed, we have nothing left to manage and we
// can clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

@end
