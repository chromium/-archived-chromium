// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_contents_controller.h"

#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/command_updater.h"
#import "chrome/browser/location_bar.h"

// For now, tab_contents lives here. TODO(port):fix
#include "chrome/common/temp_scaffolding_stubs.h"

@interface TabContentsController(CommandUpdates)
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled;
@end

@interface TabContentsController(Private)
- (void)updateToolbarCommandStatus;
@end

// A C++ bridge class that handles listening for updates to commands and
// passing them back to the controller.
class TabContentsCommandObserver : public CommandUpdater::CommandObserver {
 public:
  TabContentsCommandObserver(TabContentsController* controller,
                             CommandUpdater* commands);
  ~TabContentsCommandObserver();

  // Overridden from CommandUpdater::CommandObserver
  void EnabledStateChangedForCommand(int command, bool enabled);

 private:
  TabContentsController* controller_;  // weak, owns me
  CommandUpdater* commands_;  // weak
};

// TODO(pinkerton): implement these
class LocationBarBridge : public LocationBar {
 public:
  LocationBarBridge(TabContentsController* controller);

  // Overridden from LocationBar
  virtual void ShowFirstRunBubble() { NOTIMPLEMENTED(); }
  virtual std::wstring GetInputString() const { NOTIMPLEMENTED(); return L""; }
  virtual WindowOpenDisposition GetWindowOpenDisposition() const
      { NOTIMPLEMENTED(); return NEW_FOREGROUND_TAB; }
  virtual PageTransition::Type GetPageTransition() const 
      { NOTIMPLEMENTED(); return 0; }
  virtual void AcceptInput() { NOTIMPLEMENTED(); }
  virtual void FocusLocation() { NOTIMPLEMENTED(); }
  virtual void FocusSearch() { NOTIMPLEMENTED(); }
  virtual void SaveStateToContents(TabContents* contents) { NOTIMPLEMENTED(); }

 private:
  TabContentsController* controller_;  // weak, owns me
};

@implementation TabContentsController

- (id)initWithNibName:(NSString*)name 
               bundle:(NSBundle*)bundle
             contents:(TabContents*)contents
             commands:(CommandUpdater*)commands {
  if ((self = [super initWithNibName:name bundle:bundle])) {
    commands_ = commands;
    if (commands_)
      observer_ = new TabContentsCommandObserver(self, commands);
    locationBarBridge_ = new LocationBarBridge(self);
    [contentsBox_ setContentView:contents->GetNativeView()];
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  delete observer_;
  delete locationBarBridge_;
  [super dealloc];
}

- (void)awakeFromNib {
  // Provide a starting point since we won't get notifications if the state
  // doesn't change between tabs.
  [self updateToolbarCommandStatus];

  [locationBar_ setStringValue:@"http://dev.chromium.org"];
}

- (LocationBar*)locationBar {
  return locationBarBridge_;
}

// Returns YES if the tab represented by this controller is the front-most.
- (BOOL)isCurrentTab {
  // We're the current tab if we're in the view hierarchy, otherwise some other
  // tab is.
  return [[self view] superview] ? YES : NO;
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  // We don't need to update anything if we're not the frontmost tab.
  // TODO(pinkerton): i'm worried that observer ordering could cause the
  // notification to be sent before we've been put into the view, but we
  // appear to be called in the right order so far.
  if (![self isCurrentTab])
    return;

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

- (IBAction)fullScreen:(id)sender {
  if ([[self view] isInFullScreenMode]) {
    [[self view] exitFullScreenModeWithOptions:nil];
  } else {
    [[self view] enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
  }
}

// Set the enabled state of the buttons on the toolbar to match the state in
// the controller. We can't only rely on notifications to do this because the
// command model only assumes a single toolbar and won't send notifications if
// the state doesn't change.
- (void)updateToolbarCommandStatus {
  [backButton_ setEnabled:commands_->IsCommandEnabled(IDC_BACK) ? YES : NO];
  [forwardButton_
      setEnabled:commands_->IsCommandEnabled(IDC_FORWARD) ? YES : NO];
  [reloadStopButton_
      setEnabled:commands_->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  [starButton_ setEnabled:commands_->IsCommandEnabled(IDC_STAR) ? YES : NO];
}

- (void)willBecomeSelectedTab {
  [self updateToolbarCommandStatus];
}

@end

//--------------------------------------------------------------------------

TabContentsCommandObserver::TabContentsCommandObserver(
    TabContentsController* controller, CommandUpdater* commands)
        : controller_(controller), commands_(commands) {
  DCHECK(controller_ && commands);
  // Register for notifications about state changes for the toolbar buttons
  commands_->AddCommandObserver(IDC_BACK, this);
  commands_->AddCommandObserver(IDC_FORWARD, this);
  commands_->AddCommandObserver(IDC_RELOAD, this);
  commands_->AddCommandObserver(IDC_HOME, this);
  commands_->AddCommandObserver(IDC_STAR, this);
}

TabContentsCommandObserver::~TabContentsCommandObserver() {
  // Unregister the notifications
  commands_->RemoveCommandObserver(this);
}

void TabContentsCommandObserver::EnabledStateChangedForCommand(int command, 
                                                               bool enabled) {
  [controller_ enabledStateChangedForCommand:command 
                                     enabled:enabled ? YES : NO];
}

//--------------------------------------------------------------------------

LocationBarBridge::LocationBarBridge(TabContentsController* controller)
    : controller_(controller) {
}
