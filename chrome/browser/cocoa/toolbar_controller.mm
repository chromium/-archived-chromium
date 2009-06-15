// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#include "chrome/browser/toolbar_model.h"

// Names of images in the bundle for the star icon (normal and 'starred').
static NSString* const kStarImageName = @"star";
static NSString* const kStarredImageName = @"starred";

@implementation LocationBarFieldEditor
- (void)copy:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCopy:pb];
}

- (void)cut:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCut:pb];
}

- (void)performCopy:(NSPasteboard*)pb {
  [pb declareTypes:[NSArray array] owner:nil];
  [self writeSelectionToPasteboard:pb types:
      [NSArray arrayWithObject:NSStringPboardType]];
}

- (void)performCut:(NSPasteboard*)pb {
  [self performCopy:pb];
  [self delete:nil];
}

@end

@interface ToolbarController(Private)
- (void)initCommandStatus:(CommandUpdater*)commands;
@end

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile {
  DCHECK(model && commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:mac_util::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;

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
                                                toolbarModel_, profile_));
  [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
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

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  locationBarView_->Update(tab, shouldRestore ? true : false);
}

- (void)setStarredState:(BOOL)isStarred {
  NSString* starImageName = kStarImageName;
  if (isStarred)
    starImageName = kStarredImageName;
  [starButton_ setImage:[NSImage imageNamed:starImageName]];
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = @"go";
  NSInteger tag = IDC_GO;
  if (isLoading) {
    imageName = @"stop";
    tag = IDC_STOP;
  }
  [goButton_ setImage:[NSImage imageNamed:imageName]];
  [goButton_ setTag:tag];
}

- (id)customFieldEditorForObject:(id)obj {
  if (obj == locationBar_) {
    // Lazilly construct Field editor, Cocoa UI code always runs on the
    // same thread, so there shoudn't be a race condition here.
    if (locationBarFieldEditor_.get() == nil) {
      locationBarFieldEditor_.reset([[LocationBarFieldEditor alloc] init]);
    }

    // This needs to be called every time, otherwise notifications
    // aren't sent correctly.
    DCHECK(locationBarFieldEditor_.get());
    [locationBarFieldEditor_.get() setFieldEditor:YES];
    return locationBarFieldEditor_.get();
  }
  return nil;
}

// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews {
  return [NSArray arrayWithObjects:backButton_, forwardButton_, reloadButton_,
            starButton_, goButton_, locationBar_, nil];
}

@end
