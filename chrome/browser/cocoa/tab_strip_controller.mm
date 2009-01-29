// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_controller.h"

#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_cell.h"
#import "chrome/browser/cocoa/tab_contents_controller.h"
#import "chrome/browser/tabs/tab_strip_model.h"

// the private methods the brige object needs from the controller
@interface TabStripController(BridgeMethods)
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground;
- (void)selectTabWithContents:(TabContents*)newContents 
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture;
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
  virtual void TabClosingAt(TabContents* contents, int index);
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

- (id)initWithView:(TabStripView*)view model:(TabStripModel*)model {
  DCHECK(view && model);
  if ((self = [super init])) {
    tabView_ = view;
    model_ = model;
    bridge_ = new TabStripBridge(model, self);
    tabContentsToController_ = [[NSMutableDictionary alloc] init];
    
    // Create the new tab button separate from the nib so we can make sure
    // it's always at the end of the subview list.
    NSImage* image = [NSImage imageNamed:@"newtab"];
    NSRect frame = NSMakeRect(0, 0, [image size].width, [image size].height);
    newTabButton_ = [[NSButton alloc] initWithFrame:frame];
    [newTabButton_ setImage:image];
    [newTabButton_ setImagePosition:NSImageOnly];
    [newTabButton_ setTarget:nil];
    [newTabButton_ setAction:@selector(commandDispatch:)];
    [newTabButton_ setTag:IDC_NEW_TAB];
    [newTabButton_ setButtonType:NSMomentaryPushInButton];
    [newTabButton_ setBordered:NO];
  }
  return self;
}

- (void)dealloc {
  delete bridge_;
  [tabContentsToController_ release];
  [newTabButton_ release];
  [super dealloc];
}

// Look up the controller associated with |contents| in the map, using its
// pointer as the key into our dictionary.
- (TabContentsController*)controllerWithContents:(TabContents*)contents {
  NSValue* key = [NSValue valueWithPointer:contents];
  return [tabContentsToController_ objectForKey:key];
}

// Finds the associated TabContentsController for |contents| using the
// internal dictionary and swaps out the sole child of the contentArea to
// display its contents.
- (void)swapInTabContents:(TabContents*)contents {
  // Look up the associated controller
  TabContentsController* controller = [self controllerWithContents:contents];
  
  // Resize the new view to fit the window
  NSView* contentView = [[tabView_ window] contentView];
  NSView* newView = [controller view];
  NSRect frame = [contentView bounds];
  frame.size.height -= 14.0;
  [newView setFrame:frame];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of the contentView because we're the one who put it there.
  NSView* oldView = [[contentView subviews] objectAtIndex:0];
  [contentView replaceSubview:oldView with:newView];
}

// Create a new tab view and set its cell correctly so it draws the way we
// want it to.
- (NSButton*)newTabWithFrame:(NSRect)frame {
  NSButton* button = [[[NSButton alloc] initWithFrame:frame] autorelease];
  TabCell* cell = [[[TabCell alloc] init] autorelease];
  [button setCell:cell];
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:@"New Tab"];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTarget:self];
  [button setAction:@selector(selectTab:)];
  
  return button;
}

// Returns the number of tab buttons in the tab strip by counting the children.
// Recall the last view is the "new tab" button, so the number of tabs is one
// less than the count.
- (NSInteger)numberOfTabViews {
  return [[tabView_ subviews] count] - 1;
}

// Returns the index of the subview |view|. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view {
  NSInteger index = -1;
  const int numSubviews = [self numberOfTabViews];
  for (int i = 0; i < numSubviews; i++) {
    if ([[tabView_ subviews] objectAtIndex:i] == view)
      index = i;
  }
  return index;
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  int index = [self indexForTabView:sender];  // for testing...
  if (index >= 0 && model_->ContainsIndex(index))
    model_->SelectTabContentsAt(index, true);
}

// Return the frame for a new tab that will go to the immediate right of the 
// tab at |index|. If |index| is 0, this will be the first tab, indented so
// as to not cover the window controls.
- (NSRect)frameForNewTabAtIndex:(NSInteger)index {
  const short kIndentLeavingSpaceForControls = 66;
  const short kNewTabWidth = 160;
  const short kTabOverlap = 16;
  
  short xOffset = kIndentLeavingSpaceForControls;
  if (index > 0) {
    NSRect previousTab = [[[tabView_ subviews] objectAtIndex:index - 1] frame];
    xOffset = NSMaxX(previousTab) - kTabOverlap;
  }

  return NSMakeRect(xOffset, 0, kNewTabWidth, [tabView_ frame].size.height);
}

// Called when a notification is received from the model to insert a new tab
// at |index|.
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)index
                 inForeground:(bool)inForeground {
  DCHECK(contents);
  DCHECK(index == TabStripModel::kNoTab || model_->ContainsIndex(index));
  
  // TODO(pinkerton): handle tab dragging in here

  // Make a new tab. Load the contents of this tab from the nib and associate
  // the new controller with |contents| so it can be looked up later.
  // TODO(pinkerton): will eventually need to pass |contents| to the 
  // controller to complete hooking things up.
  TabContentsController* contentsController =
      [[[TabContentsController alloc] initWithNibName:@"TabContents" bundle:nil]
          autorelease];
  NSValue* key = [NSValue valueWithPointer:contents];
  [tabContentsToController_ setObject:contentsController forKey:key];
  
  // Remove the new tab button so the only views present are the tabs,
  // we'll add it back when we're done
  [newTabButton_ removeFromSuperview];
  
  // Make a new tab view and add it to the strip.
  // TODO(pinkerton): move everyone else over and animate. Also will need to
  // move the "add tab" button over.
  NSRect newTabFrame = [self frameForNewTabAtIndex:index];
  NSButton* newView = [self newTabWithFrame:newTabFrame];
  [tabView_ addSubview:newView];
  
  // Add the new tab button back in to the right of the last tab.
  const NSInteger kNewTabXOffset = 10;
  NSRect lastTab = 
    [[[tabView_ subviews] objectAtIndex:[[tabView_ subviews] count] - 1] frame];
  NSInteger maxRightEdge = NSMaxX(lastTab);
  NSRect newTabButtonFrame = [newTabButton_ frame];
  newTabButtonFrame.origin.x = maxRightEdge + kNewTabXOffset;
  [newTabButton_ setFrame:newTabButtonFrame];
  [tabView_ addSubview:newTabButton_];
  
  // Select the newly created tab if in the foreground
  if (inForeground)
    [self swapInTabContents:contents];
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)selectTabWithContents:(TabContents*)newContents 
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  // De-select all other tabs and select the new tab.
  const int numSubviews = [self numberOfTabViews];
  for (int i = 0; i < numSubviews; i++) {
    NSButton* current = [[tabView_ subviews] objectAtIndex:i];
    [current setState:(i == index) ? NSOnState : NSOffState];
  }
  
  // Swap in the contents for the new tab
  [self swapInTabContents:newContents];
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

void TabStripBridge::TabClosingAt(TabContents* contents, int index) {
}

void TabStripBridge::TabDetachedAt(TabContents* contents, int index) {
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
}

void TabStripBridge::TabChangedAt(TabContents* contents, int index) {
}

void TabStripBridge::TabStripEmpty() {
}
