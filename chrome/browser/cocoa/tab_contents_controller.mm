// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_contents_controller.h"

#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/command_updater.h"

@interface TabContentsController(CommandUpdates)
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled;
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
  TabContentsController* controller_;  // weak, owns us
  CommandUpdater* commands_;  // weak
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
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  delete observer_;
  [super dealloc];
}

- (void)awakeFromNib {
  [locationBar_ setStringValue:@"http://dev.chromium.org"];
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
  commands_->RemoveCommandObserver(IDC_BACK, this);
  commands_->RemoveCommandObserver(IDC_FORWARD, this);
  commands_->RemoveCommandObserver(IDC_RELOAD, this);
  commands_->RemoveCommandObserver(IDC_HOME, this);
  commands_->RemoveCommandObserver(IDC_STAR, this);
}

void TabContentsCommandObserver::EnabledStateChangedForCommand(int command, 
                                                               bool enabled) {
  [controller_ enabledStateChangedForCommand:command 
                                     enabled:enabled ? YES : NO];
}
