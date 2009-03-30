// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_contents_controller.h"

#import "base/sys_string_conversions.h"
#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/command_updater.h"
#import "chrome/browser/location_bar.h"
#import "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/toolbar_model.h"
#import "chrome/browser/net/url_fixer_upper.h"

// For now, tab_contents lives here. TODO(port):fix
#include "chrome/common/temp_scaffolding_stubs.h"

// Names of images in the bundle for the star icon (normal and 'starred').
static NSString* const kStarImageName = @"star";
static NSString* const kStarredImageName = @"starred";

@interface TabContentsController(CommandUpdates)
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled;
@end

@interface TabContentsController(LocationBar)
- (NSString*)locationBarString;
- (void)focusLocationBar;
@end

@interface TabContentsController(Private)
- (void)updateToolbarCommandStatus;
- (void)applyContentsBoxOffset:(BOOL)apply;
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

// A C++ bridge class that handles responding to requests from the
// cross-platform code for information about the location bar. Just passes
// everything back to the controller.
class LocationBarBridge : public LocationBar {
 public:
  LocationBarBridge(TabContentsController* controller);

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
  TabContentsController* controller_;  // weak, owns me
};

@implementation TabContentsController

- (id)initWithNibName:(NSString*)name
               bundle:(NSBundle*)bundle
             contents:(TabContents*)contents
             commands:(CommandUpdater*)commands
         toolbarModel:(ToolbarModel*)toolbarModel
        bookmarkModel:(BookmarkModel*)bookmarkModel {
  if ((self = [super initWithNibName:name bundle:bundle])) {
    commands_ = commands;
    if (commands_)
      observer_ = new TabContentsCommandObserver(self, commands);
    locationBarBridge_ = new LocationBarBridge(self);
    contents_ = contents;
    toolbarModel_ = toolbarModel;
    bookmarkModel_ = bookmarkModel;
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
  [contentsBox_ setContentView:contents_->GetNativeView()];
  [self applyContentsBoxOffset:YES];

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
  [reloadButton_
      setEnabled:commands_->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  [starButton_ setEnabled:commands_->IsCommandEnabled(IDC_STAR) ? YES : NO];
}

- (void)willBecomeSelectedTab {
  [self updateToolbarCommandStatus];
}

- (void)tabDidChange:(TabContents*)updatedContents {
  contents_ = updatedContents;
  [contentsBox_ setContentView:contents_->GetNativeView()];
}

- (NSString*)locationBarString {
  return [locationBar_ stringValue];
}

- (void)focusLocationBar {
  [[locationBar_ window] makeFirstResponder:locationBar_];
}

- (void)updateToolbarWithContents:(TabContents*)tab {
  // TODO(pinkerton): there's a lot of ui code in autocomplete_edit.cc
  // that we'll want to duplicate. For now, just handle setting the text.

  // TODO(pinkerton): update the security lock icon and background color

  NSString* urlString = base::SysWideToNSString(toolbarModel_->GetText());
  [locationBar_ setStringValue:urlString];
}

- (void)setStarredState:(BOOL)isStarred {
  NSString* starImageName = kStarImageName;
  if (isStarred)
    starImageName = kStarredImageName;
  [starButton_ setImage:[NSImage imageNamed:starImageName]];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of this tab.
- (NSRect)growBoxRect {
  NSRect localGrowBox = NSMakeRect(0, 0, 0, 0);
  NSView* contentView = contents_->GetNativeView();
  if (contentView) {
    // For the rect, we start with the grow box view which is a sibling of
    // the content view's containing box. It's in the coordinate system of
    // the controller view.
    localGrowBox = [growBox_ frame];
    // The scrollbar assumes that the resizer goes all the way down to the
    // bottom corner, so we ignore any y offset to the rect itself and use the
    // entire bottom corner.
    localGrowBox.origin.y = 0;
    // Convert to the content view's coordinates.
    localGrowBox = [contentView convertRect:localGrowBox
                                   fromView:[self view]];
    // Flip the rect in view coordinates
    localGrowBox.origin.y =
        [contentView frame].size.height - localGrowBox.origin.y -
            localGrowBox.size.height;
  }
  return localGrowBox;
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = @"go";
  if (isLoading)
    imageName = @"stop";
  [goButton_ setImage:[NSImage imageNamed:imageName]];
}

- (void)toggleBookmarkBar:(BOOL)enable {
  contentsBoxHasOffset_ = enable;
  [self applyContentsBoxOffset:enable];

  if (enable) {
    // TODO(jrg): display something useful in the bookmark bar
    // TODO(jrg): use a BookmarksView, not a ToolbarView
    // TODO(jrg): don't draw a border around it
    // TODO(jrg): ...
  }
}

// Apply a contents box offset to make (or remove) room for the
// bookmark bar.  If apply==YES, always make room (the contentsBox_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
- (void)applyContentsBoxOffset:(BOOL)apply {

  if (bookmarkView_ == nil) {
    // We're too early, but awakeFromNib will call me again.
    return;
  }
  if (!contentsBoxHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  int offset = [bookmarkView_ frame].size.height;
  NSRect frame = [contentsBox_ frame];
  if (apply)
    frame.size.height -= offset;
  else
    frame.size.height += offset;

  // TODO(jrg): animate
  [contentsBox_ setFrame:frame];

  [bookmarkView_ setNeedsDisplay:YES];
  [contentsBox_ setNeedsDisplay:YES];
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

std::wstring LocationBarBridge::GetInputString() const {
  // TODO(shess): This code is temporary until the omnibox code takes
  // over.
  std::wstring url = base::SysNSStringToWide([controller_ locationBarString]);

  // Try to flesh out the input to make a real URL.
  return URLFixerUpper::FixupURL(url, std::wstring());
}

void LocationBarBridge::FocusLocation() {
  [controller_ focusLocationBar];
}
