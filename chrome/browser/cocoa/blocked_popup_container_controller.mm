// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#import "chrome/browser/cocoa/blocked_popup_container_controller.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"

#import "chrome/browser/cocoa/background_gradient_view.h"

// A C++ bridge class that manages the interaction between the C++ interface
// and the Objective-C view controller that implements the popup blocker.
class BlockedPopupContainerViewBridge : public BlockedPopupContainerView {
 public:
  BlockedPopupContainerViewBridge(BlockedPopupContainerController* controller);
  virtual ~BlockedPopupContainerViewBridge();

  // Overrides from BlockedPopupContainerView
  virtual void SetPosition();
  virtual void ShowView();
  virtual void UpdateLabel();
  virtual void HideView();
  virtual void Destroy();

 private:
  BlockedPopupContainerController* controller_;  // Weak, owns us.
};

@interface BlockedPopupContainerController(Private)
- (void)initPopupView;
- (NSView*)containingView;
@end

@implementation BlockedPopupContainerController

// Initialize with the given popup container. Creates the C++ bridge object
// used to represent the "view".
- (id)initWithContainer:(BlockedPopupContainer*)container {
  if ((self = [super init])) {
    container_ = container;
    bridge_.reset(new BlockedPopupContainerViewBridge(self));
    [self initPopupView];
  }
  return self;
}

- (void)dealloc {
  [view_ removeFromSuperview];
  [super dealloc];
}

- (IBAction)closePopup:(id)sender {
  container_->set_dismissed();
  container_->CloseAll();
}

// Create and initialize the popup view and its label, close box, etc.
- (void)initPopupView {
  static const float kWidth = 200.0;
  static const float kHeight = 20.0;
  static const float kCloseBoxSize = 16.0;
  static const float kCloseBoxPaddingY = 2.0;
  static const float kLabelPaddingX = 5.0;

  // Create it below the parent's bottom edge so we can animate it into place.
  NSRect startFrame = NSMakeRect(0.0, -kHeight, kWidth, kHeight);
  view_.reset([[BackgroundGradientView alloc] initWithFrame:startFrame]);
  [view_ setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];

  // Create the text label and position it. We'll resize it later when the
  // label gets updated. The view owns the label, we only hold a weak reference.
  NSRect labelFrame = NSMakeRect(kLabelPaddingX,
                                 0,
                                 startFrame.size.width - kCloseBoxSize,
                                 startFrame.size.height);
  label_ = [[[NSTextField alloc] initWithFrame:labelFrame] autorelease];
  [label_ setSelectable:NO];
  [label_ setAutoresizingMask:NSViewWidthSizable];
  [label_ setBordered:NO];
  [label_ setBezeled:NO];
  [label_ setDrawsBackground:NO];
  [view_ addSubview:label_];

  // Create the close box and position at the left of the view.
  NSRect closeFrame = NSMakeRect(startFrame.size.width - kCloseBoxSize,
                                 kCloseBoxPaddingY,
                                 kCloseBoxSize,
                                 kCloseBoxSize);
  NSButton* close = [[[NSButton alloc] initWithFrame:closeFrame] autorelease];
  [close setAutoresizingMask:NSViewMinXMargin];
  [close setImage:[NSImage imageNamed:@"close_bar"]];
  [close setAlternateImage:[NSImage imageNamed:@"close_bar_p"]];
  [close setBordered:NO];
  [close setTarget:self];
  [close setAction:@selector(closePopup:)];
  [view_ addSubview:close];
}

// Returns the C++ brige object.
- (BlockedPopupContainerView*)bridge {
  return bridge_.get();
}

// Returns the parent of the RWHVMac. We insert our popup blocker as a sibling
// so that it stays around as the RWHVMac is created/destroyed during
// navigation.
- (NSView*)containingView {
  return container_->GetConstrainingContents(NULL)->view()->GetNativeView();
}

- (void)show {
  const float kLeftPadding = 20;  // Leave room for the scrollbar.

  // No need to do anything if it's already on screen.
  if ([view_ superview]) return;

  // Position the view at the bottom right corner, always leaving room for the
  // scrollbar. This is what window does. It also doesn't care about covering
  // up the horizontal scrollbar.
  NSView* parent = [self containingView];
  NSRect frame = [view_ frame];
  frame.origin.x = [parent frame].size.width - frame.size.width - kLeftPadding;
  [view_ setFrame:frame];

  // Add the view and animate it sliding up into view.
  [parent addSubview:view_.get()];
  frame.origin.y = 0;
  [[view_ animator] setFrame:frame];
}

- (void)hide {
  [view_ removeFromSuperview];
}

// Resize the view based on the new label contents. The autoresize mask will
// take care of resizing everything else.
- (void)resizeWithLabel:(NSString*)label {
#if 0
// TODO(pinkerton): fix this once the popup gets put in.
  NSSize stringSize = [label sizeWithAttributes:nil];
  NSRect frame = [view_ frame];
  float originalWidth = frame.size.width;
  frame.size.width = stringSize.width + 16 + 5;
  frame.origin.x -= frame.size.width - originalWidth;
  [view_ setFrame:frame];
#endif
}

- (void)update {
  size_t blockedPopups = container_->GetBlockedPopupCount();
  NSString* label = nil;
  if (blockedPopups) {
    label = base::SysUTF16ToNSString(
        l10n_util::GetStringFUTF16(IDS_POPUPS_BLOCKED_COUNT,
                                   UintToString16(blockedPopups)));
  } else {
    label = base::SysUTF16ToNSString(
        l10n_util::GetStringUTF16(IDS_POPUPS_UNBLOCKED));
  }
  [self resizeWithLabel:label];
  [label_ setStringValue:label];
}

- (NSView*)view {
  return view_.get();
}

- (NSView*)label {
  return label_;
}

@end

//---------------------------------------------------------------------------

BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  // We "leak" |blocker| for now, we'll release it when the bridge class
  // gets a Destroy() message.
  BlockedPopupContainerController* blocker =
      [[BlockedPopupContainerController alloc] initWithContainer:container];
  return [blocker bridge];
}

BlockedPopupContainerViewBridge::BlockedPopupContainerViewBridge(
    BlockedPopupContainerController* controller) {
  controller_ = controller;
}

BlockedPopupContainerViewBridge::~BlockedPopupContainerViewBridge() {
}

void BlockedPopupContainerViewBridge::SetPosition() {
  // Doesn't ever get called, also a no-op on GTK.
  NOTIMPLEMENTED();
}

void BlockedPopupContainerViewBridge::ShowView() {
  [controller_ show];
}

void BlockedPopupContainerViewBridge::UpdateLabel() {
  [controller_ update];
}

void BlockedPopupContainerViewBridge::HideView() {
  [controller_ hide];
}

void BlockedPopupContainerViewBridge::Destroy() {
  [controller_ autorelease];
}
