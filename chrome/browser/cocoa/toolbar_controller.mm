// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#include "chrome/browser/toolbar_model.h"

// Names of images in the bundle for the star icon (normal and 'starred').
static NSString* const kStarImageName = @"star";
static NSString* const kStarredImageName = @"starred";

@interface ToolbarController(Private)
- (void)initCommandStatus:(CommandUpdater*)commands;
@end

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands {
  DCHECK(model && commands);
  if ((self = [super initWithNibName:@"Toolbar" bundle:nil])) {
    toolbarModel_ = model;
    commands_ = commands;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(new CommandObserverBridge(self, commands));
    commandObserver_->ObserveCommand(IDC_BACK);
    commandObserver_->ObserveCommand(IDC_FORWARD);
    commandObserver_->ObserveCommand(IDC_RELOAD);
    commandObserver_->ObserveCommand(IDC_HOME);
    commandObserver_->ObserveCommand(IDC_STAR);
  }
  return self;
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location
// bar and button state.
- (void)awakeFromNib {
  [self initCommandStatus:commands_];
  locationBarView_.reset(new LocationBarViewMac(locationBar_, commands_,
                                                toolbarModel_));
  locationBarView_->Init();
  [locationBar_ setStringValue:@"http://dev.chromium.org"];
}

- (void)dealloc {
  [super dealloc];
}

- (LocationBar*)locationBar {
  return locationBarView_.get();
}

- (void)focusLocationBar {
  if (locationBarView_.get()) {
    locationBarView_->FocusLocation();
  }
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  NSButton* button = nil;
  switch (command) {
    case IDC_BACK:
      button = backButton_;
      break;
    case IDC_FORWARD:
      button = forwardButton_;
      break;
    case IDC_HOME:
      // TODO(pinkerton): add home button
      break;
    case IDC_STAR:
      button = starButton_;
      break;
  }
  [button setEnabled:enabled];
}

// Init the enabled state of the buttons on the toolbar to match the state in
// the controller.
- (void)initCommandStatus:(CommandUpdater*)commands {
  [backButton_ setEnabled:commands->IsCommandEnabled(IDC_BACK) ? YES : NO];
  [forwardButton_
      setEnabled:commands->IsCommandEnabled(IDC_FORWARD) ? YES : NO];
  [reloadButton_
      setEnabled:commands->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  // TODO(pinkerton): Add home button.
  [starButton_ setEnabled:commands->IsCommandEnabled(IDC_STAR) ? YES : NO];
}

- (void)updateToolbarWithContents:(TabContents*)tab {
  // TODO(pinkerton): there's a lot of ui code in autocomplete_edit.cc
  // that we'll want to duplicate. For now, just handle setting the text.

  // TODO(shess): This is the start of what pinkerton refers to.
  // Unfortunately, I'm going to need to spend some time wiring things
  // up.  This call should suffice to close any open autocomplete
  // pulldown.  It should also be the right thing to do to save and
  // restore state, but at this time it's not clear that this is the
  // right place, and tab is not the right parameter.
  if (locationBarView_.get()) {
    locationBarView_->SaveStateToContents(NULL);
  }

  // TODO(pinkerton): update the security lock icon and background color

  // TODO(shess): Determine whether this should happen via
  // locationBarView_, instead, in which case this class can
  // potentially lose the locationBar_ reference.
  NSString* urlString = base::SysWideToNSString(toolbarModel_->GetText());
  [locationBar_ setStringValue:urlString];
}

- (void)setStarredState:(BOOL)isStarred {
  NSString* starImageName = kStarImageName;
  if (isStarred)
    starImageName = kStarredImageName;
  [starButton_ setImage:[NSImage imageNamed:starImageName]];
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = @"go";
  if (isLoading)
    imageName = @"stop";
  [goButton_ setImage:[NSImage imageNamed:imageName]];
}

@end
