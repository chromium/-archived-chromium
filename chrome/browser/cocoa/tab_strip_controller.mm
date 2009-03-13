// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_controller.h"

#import "base/sys_string_conversions.h"
#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_cell.h"
#import "chrome/browser/cocoa/tab_contents_controller.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/tabs/tab_strip_model.h"

// The amount of overlap tabs have in their button frames.
const short kTabOverlap = 16;

// The private methods the brige object needs from the controller.
@interface TabStripController(BridgeMethods)
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground;
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture;
- (void)tabDetachedWithContents:(TabContents*)contents
                        atIndex:(NSInteger)index;
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index;
@end

// A C++ bridge class to handle receiving notifications from the C++ tab
// strip model. Doesn't do much on its own, just sends everything straight
// to the Cocoa controller.
class TabStripBridge : public TabStripModelObserver {
 public:
  TabStripBridge(TabStripModel* model, TabStripController* controller);
  ~TabStripBridge();

  // Overridden from TabStripModelObserver
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index);
  virtual void TabStripEmpty();

 private:
  TabStripController* controller_;  // weak, owns me
  TabStripModel* model_;  // weak, owned by Browser
};

@implementation TabStripController

- (id)initWithView:(TabStripView*)view
          tabModel:(TabStripModel*)tabModel
      toolbarModel:(ToolbarModel*)toolbarModel
          commands:(CommandUpdater*)commands {
  DCHECK(view && tabModel && toolbarModel);
  if ((self = [super init])) {
    tabView_ = view;
    tabModel_ = tabModel;
    toolbarModel_ = toolbarModel;
    commands_ = commands;
    bridge_ = new TabStripBridge(tabModel, self);
    tabContentsArray_ = [[NSMutableArray alloc] init];
    tabArray_ = [[NSMutableArray alloc] init];

    // Take the only child view present in the nib as the new tab button. For
    // some reason, if the view is present in the nib apriori, it draws
    // correctly. If we create it in code and add it to the tab view, it draws
    // with all sorts of crazy artifacts.
    newTabButton_ = [[tabView_ subviews] objectAtIndex:0];
    DCHECK([newTabButton_ isKindOfClass:[NSButton class]]);
    [newTabButton_ setTarget:nil];
    [newTabButton_ setAction:@selector(commandDispatch:)];
    [newTabButton_ setTag:IDC_NEW_TAB];

    [tabView_ setWantsLayer:YES];
  }
  return self;
}

- (void)dealloc {
  delete bridge_;
  [tabContentsArray_ release];
  [tabArray_ release];
  [super dealloc];
}

// Finds the associated TabContentsController at the given |index| and swaps
// out the sole child of the contentArea to display its contents.
- (void)swapInTabAtIndex:(NSInteger)index {
  TabContentsController* controller = [tabContentsArray_ objectAtIndex:index];

  // Resize the new view to fit the window
  NSView* contentView = [[tabView_ window] contentView];
  NSView* newView = [controller view];
  NSRect frame = [contentView bounds];
  frame.size.height -= 14.0;
  [newView setFrame:frame];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of the contentView because we're the one who put it there. There
  // may not be any children in the case of a tab that's been closed, in
  // which case there's no swapping going on.
  NSArray* subviews = [contentView subviews];
  if ([subviews count]) {
    NSView* oldView = [subviews objectAtIndex:0];
    [contentView replaceSubview:oldView with:newView];
  } else {
    [contentView addSubview:newView];
  }
}

// Create a new tab view and set its cell correctly so it draws the way we
// want it to.
- (TabController*)newTabWithFrame:(NSRect)frame {
  TabController* controller = [[[TabController alloc] init] autorelease];
  [controller setTarget:self];
  [controller setAction:@selector(selectTab:)];
  TabView* view = [controller tabView];
  [view setFrame:frame];

  return controller;
}

// Returns the number of tabs in the tab strip. This is just the number
// of TabControllers we know about as there's a 1-to-1 mapping from these
// controllers to a tab.
- (NSInteger)numberOfTabViews {
  return [tabArray_ count];
}

// Returns the index of the subview |view|. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view {
  NSInteger index = 0;
  for (TabController* current in tabArray_) {
    if ([current view] == view)
      return index;
    ++index;
  }
  return -1;
}

// Returns the view at the given index, using the array of TabControllers to
// get the associated view. Returns nil if out of range.
- (NSView*)viewAtIndex:(NSUInteger)index {
  if (index >= [tabArray_ count])
    return NULL;
  return [[tabArray_ objectAtIndex:index] view];
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  int index = [self indexForTabView:sender];  // for testing...
  if (index >= 0 && tabModel_->ContainsIndex(index))
    tabModel_->SelectTabContentsAt(index, true);
}

// Return the frame for a new tab that will go to the immediate right of the
// tab at |index|. If |index| is 0, this will be the first tab, indented so
// as to not cover the window controls.
- (NSRect)frameForNewTabAtIndex:(NSInteger)index {
  const short kIndentLeavingSpaceForControls = 66;
  const short kNewTabWidth = [TabController maxTabWidth];

  short xOffset = kIndentLeavingSpaceForControls;
  if (index > 0) {
    NSRect previousTab = [[self viewAtIndex:index - 1] frame];
    xOffset = NSMaxX(previousTab) - kTabOverlap;
  }

  return NSMakeRect(xOffset, 0, kNewTabWidth, [tabView_ frame].size.height);
}

// Positions the new tab button to the right of the last tab.
- (void)positionNewTabButton {
  const NSInteger kNewTabXOffset = -12;
  NSRect lastTab = [[[tabArray_ lastObject] view] frame];
  NSInteger maxRightEdge = NSMaxX(lastTab);
  NSRect newTabButtonFrame = [newTabButton_ frame];
  newTabButtonFrame.origin.x = maxRightEdge + kNewTabXOffset;
  [newTabButton_ setFrame:newTabButtonFrame];
  [newTabButton_ setHidden:NO];
}

// Handles setting the title of the tab based on the given |contents|. Uses
// a canned string if |contents| is NULL.
- (void)setTabTitle:(NSViewController*)tab withContents:(TabContents*)contents {
  NSString* titleString = nil;
  if (contents)
    titleString = base::SysUTF16ToNSString(contents->GetTitle());
  if (![titleString length])
    titleString = NSLocalizedString(@"untitled", nil);
  [tab setTitle:titleString];
}

// Called when a notification is received from the model to insert a new tab
// at |index|.
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground {
  DCHECK(contents);
  DCHECK(index == TabStripModel::kNoTab || tabModel_->ContainsIndex(index));

  // TODO(pinkerton): handle tab dragging in here

  // Make a new tab. Load the contents of this tab from the nib and associate
  // the new controller with |contents| so it can be looked up later.
  TabContentsController* contentsController =
      [[[TabContentsController alloc] initWithNibName:@"TabContents"
                                               bundle:nil
                                             contents:contents
                                             commands:commands_
                                         toolbarModel:toolbarModel_]
          autorelease];
  [tabContentsArray_ insertObject:contentsController atIndex:index];

  // Make a new tab and add it to the strip. Keep track of its controller.
  // TODO(pinkerton): move everyone else over and animate. Also will need to
  // move the "add tab" button over.
  NSRect newTabFrame = [self frameForNewTabAtIndex:index];
  TabController* newController = [self newTabWithFrame:newTabFrame];
  [tabArray_ insertObject:newController atIndex:index];
  NSView* newView = [newController view];
  [tabView_ addSubview:newView];

  [self positionNewTabButton];

  [self setTabTitle:newController withContents:contents];

  // Select the newly created tab if in the foreground
  if (inForeground)
    [self swapInTabAtIndex:index];
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  // De-select all other tabs and select the new tab.
  int i = 0;
  for (TabController* current in tabArray_) {
    [current setSelected:(i == index) ? YES : NO];
    ++i;
  }

  // Make this the top-most tab in the strips's z order.
  NSView* selectedTab = [self viewAtIndex:index];
  [tabView_ addSubview:selectedTab positioned:NSWindowAbove relativeTo:nil];

  // Tell the new tab contents it is about to become the selected tab. Here it
  // can do things like make sure the toolbar is up to date.
  TabContentsController* newController =
      [tabContentsArray_ objectAtIndex:index];
  [newController willBecomeSelectedTab];

  // Swap in the contents for the new tab
  [self swapInTabAtIndex:index];
}

// Called when a notification is received from the model that the given tab
// has gone away. Remove all knowledge about this tab and it's associated
// controller and remove the view from the strip.
- (void)tabDetachedWithContents:(TabContents*)contents
                        atIndex:(NSInteger)index {
  // Release the tab contents controller so those views get destroyed. This
  // will remove all the tab content Cocoa views from the hierarchy. A
  // subsequent "select tab" notification will follow from the model. To
  // tell us what to swap in in its absence.
  [tabContentsArray_ removeObjectAtIndex:index];

  // Remove the |index|th view from the tab strip
  NSView* tab = [self viewAtIndex:index];
  NSInteger tabWidth = [tab frame].size.width;
  [tab removeFromSuperview];

  // Move all the views that are to the right of the tab being removed over
  // the width of the tab that was closed. Don't bother animating if there is
  // only 1 tab as everything is going away.
  if ([self numberOfTabViews] > 1) {
    int currIndex = 0;
    for (TabController* curr in tabArray_) {
      if (currIndex > index) {
        NSView* shiftingView = [curr view];
        NSRect newFrame = [shiftingView frame];
        newFrame.origin.x -= tabWidth - kTabOverlap;
        [[shiftingView animator] setFrame:newFrame];
      }
      ++currIndex;
    }

    // Move the new tab button. Note we can't just use the position of the
    // last tab because it will still be at the old location due to the delay
    // due to animation.
    NSRect newTabFrame = [newTabButton_ frame];
    newTabFrame.origin.x -= tabWidth - kTabOverlap;
    [[newTabButton_ animator] setFrame:newTabFrame];
  }

  // Once we're totally done with the tab, delete its controller
  [tabArray_ removeObjectAtIndex:index];
}

// Called when a notification is received from the model that the given tab
// has been updated.
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index {
  [self setTabTitle:[tabArray_ objectAtIndex:index] withContents:contents];

  TabContentsController* updatedController =
      [tabContentsArray_ objectAtIndex:index];
  [updatedController tabDidChange:contents];
}

- (LocationBar*)locationBar {
  TabContentsController* selectedController =
      [tabContentsArray_ objectAtIndex:tabModel_->selected_index()];
  return [selectedController locationBar];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  // TODO(pinkerton): OS_WIN maintains this, but I'm not sure why. It's
  // available by querying the model, which we have available to us.
  currentTab_ = tab;

  // tell the appropriate controller to update its state. |shouldRestore| being
  // YES means we're going back to this tab and should put back any state
  // associated with it.
  TabContentsController* controller =
      [tabContentsArray_ objectAtIndex:tabModel_->GetIndexOfTabContents(tab)];
  [controller updateToolbarWithContents:shouldRestore ? tab : nil];
}

- (void)setStarredState:(BOOL)isStarred {
  TabContentsController* selectedController =
      [tabContentsArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect {
  int selectedIndex = tabModel_->selected_index();
  if (selectedIndex == TabStripModel::kNoTab) {
    // When the window is initially being constructed, there may be no currently
    // selected tab, so pick the first one. If there aren't any, just bail with
    // an empty rect.
    selectedIndex = 0;
  }
  TabContentsController* selectedController =
      [tabContentsArray_ objectAtIndex:selectedIndex];
  if (!selectedController)
    return NSZeroRect;
  return [selectedController growBoxRect];
}

// Called to tell the selected tab to update its loading state.
- (void)setIsLoading:(BOOL)isLoading {
  // TODO(pinkerton): update the favicon on the selected tab view to/from
  // a spinner?

  TabContentsController* selectedController =
      [tabContentsArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController setIsLoading:isLoading];
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar {
  TabContentsController* selectedController =
      [tabContentsArray_ objectAtIndex:tabModel_->selected_index()];
  [selectedController focusLocationBar];
}

@end

//--------------------------------------------------------------------------

TabStripBridge::TabStripBridge(TabStripModel* model,
                               TabStripController* controller)
    : controller_(controller), model_(model) {
  // Register to be a listener on the model so we can get updates and tell
  // the TabStripController about them.
  model_->AddObserver(this);
}

TabStripBridge::~TabStripBridge() {
  // Remove ourselves from receiving notifications.
  model_->RemoveObserver(this);
}

void TabStripBridge::TabInsertedAt(TabContents* contents,
                                   int index,
                                   bool foreground) {
  [controller_ insertTabWithContents:contents
                             atIndex:index
                        inForeground:foreground];
}

void TabStripBridge::TabDetachedAt(TabContents* contents, int index) {
  [controller_ tabDetachedWithContents:contents atIndex:index];
}

void TabStripBridge::TabSelectedAt(TabContents* old_contents,
                                   TabContents* new_contents,
                                   int index,
                                   bool user_gesture) {
  [controller_ selectTabWithContents:new_contents
                    previousContents:old_contents
                             atIndex:index
                         userGesture:user_gesture];
}

void TabStripBridge::TabMoved(TabContents* contents,
                              int from_index,
                              int to_index) {
  NOTIMPLEMENTED();
}

void TabStripBridge::TabChangedAt(TabContents* contents, int index) {
  [controller_ tabChangedWithContents:contents atIndex:index];
}

void TabStripBridge::TabStripEmpty() {
  NOTIMPLEMENTED();
}
