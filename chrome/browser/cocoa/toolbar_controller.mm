// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

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
- (void)prefChanged:(std::wstring*)prefName;
@end

namespace ToolbarControllerInternal {

// A C++ class registered for changes in preferences. Bridges the
// notification back to the ToolbarController.
class PrefObserverBridge : public NotificationObserver {
 public:
  PrefObserverBridge(ToolbarController* controller)
      : controller_(controller) { }
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [controller_ prefChanged:Details<std::wstring>(details).ptr()];
  }
 private:
  ToolbarController* controller_;  // weak, owns us
};

}  // namespace

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
     webContentView:(NSView*)webContentView
   bookmarkDelegate:(id<BookmarkURLOpener>)delegate {
  DCHECK(model && commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:mac_util::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;
    bookmarkBarDelegate_ = delegate;
    webContentView_ = webContentView;

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

  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar them based on those prefs.
  prefObserver_.reset(new ToolbarControllerInternal::PrefObserverBridge(self));
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(prefs::kShowHomeButton, prefs, prefObserver_.get());
  showPageOptionButtons_.Init(prefs::kShowPageOptionsButtons, prefs,
                              prefObserver_.get());
  [self showOptionalHomeButton];
  [self showOptionalPageWrenchButtons];

  // Create a sub-controller for the bookmark bar.
  bookmarkBarController_.reset([[BookmarkBarController alloc]
                                   initWithProfile:profile_
                                              view:bookmarkBarView_
                                    webContentView:webContentView_
                                          delegate:bookmarkBarDelegate_]);
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
      button = homeButton_;
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
  [reloadButton_ setEnabled:commands->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  [homeButton_ setEnabled:commands->IsCommandEnabled(IDC_HOME) ? YES : NO];
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

- (BookmarkBarController*)bookmarkBarController {
  return bookmarkBarController_.get();
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
            homeButton_, starButton_, goButton_, pageButton_, wrenchButton_,
            locationBar_, bookmarkBarView_, nil];
}

// Moves |rect| to the right by |delta|, keeping the right side fixed by
// shrinking the width to compensate. Passing a negative value for |deltaX|
// moves to the left and increases the width.
- (NSRect)adjustRect:(NSRect)rect byAmount:(float)deltaX {
  NSRect frame = NSOffsetRect(rect, deltaX, 0);
  frame.size.width -= deltaX;
  return frame;
}

// Computes the padding between the buttons that should have a separation from
// the positions in the nib. Since the forward and reload buttons are always
// visible, we use those buttons as the canonical spacing.
- (float)interButtonSpacing {
  NSRect forwardFrame = [forwardButton_ frame];
  NSRect reloadFrame = [reloadButton_ frame];
  DCHECK(NSMinX(reloadFrame) > NSMaxX(forwardFrame));
  return NSMinX(reloadFrame) - NSMaxX(forwardFrame);
}

// Show or hide the home button based on the pref.
- (void)showOptionalHomeButton {
  BOOL hide = showHomeButton_.GetValue() ? NO : YES;
  if (hide == [homeButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Always shift the star and text field by the width of the home button plus
  // the appropriate gap width. If we're hiding the button, we have to
  // reverse the direction of the movement (to the left).
  float moveX = [self interButtonSpacing] + [homeButton_ frame].size.width;
  if (hide)
    moveX *= -1;  // Reverse the direction of the move.

  [starButton_ setFrame:NSOffsetRect([starButton_ frame], moveX, 0)];
  [locationBar_ setFrame:[self adjustRect:[locationBar_ frame]
                                 byAmount:moveX]];
  [homeButton_ setHidden:hide];
}

// Show or hide the page and wrench buttons based on the pref.
- (void)showOptionalPageWrenchButtons {
  DCHECK([pageButton_ isHidden] == [wrenchButton_ isHidden]);
  BOOL hide = showPageOptionButtons_.GetValue() ? NO : YES;
  if (hide == [pageButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Shift the go button and resize the text field by the width of the
  // page/wrench buttons plus two times the gap width. If we're showing the
  // buttons, we have to reverse the direction of movement (to the left). Unlike
  // the home button above, we only ever have to resize the text field, we don't
  // have to move it.
  float moveX = 2 * [self interButtonSpacing] + NSWidth([pageButton_ frame]) +
                  NSWidth([wrenchButton_ frame]);
  if (!hide)
    moveX *= -1;  // Reverse the direction of the move.
  [goButton_ setFrame:NSOffsetRect([goButton_ frame], moveX, 0)];
  NSRect locationFrame = [locationBar_ frame];
  locationFrame.size.width += moveX;
  [locationBar_ setFrame:locationFrame];

  [pageButton_ setHidden:hide];
  [wrenchButton_ setHidden:hide];
}

- (void)prefChanged:(std::wstring*)prefName {
  if (!prefName) return;
  if (*prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  } else if (*prefName == prefs::kShowPageOptionsButtons) {
    [self showOptionalPageWrenchButtons];
  }
}

- (IBAction)showPageMenu:(id)sender {
  [NSMenu popUpContextMenu:pageMenu_
                 withEvent:[NSApp currentEvent]
                   forView:pageButton_];
}

- (IBAction)showWrenchMenu:(id)sender {
  [NSMenu popUpContextMenu:wrenchMenu_
                 withEvent:[NSApp currentEvent]
                   forView:wrenchButton_];
}

@end
