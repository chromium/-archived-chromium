// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#import "chrome/browser/cocoa/blocked_popup_container_controller.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
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
  [[NSNotificationCenter defaultCenter] removeObserver:self];
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
  popupButton_ = [[[NSPopUpButton alloc] initWithFrame:labelFrame] autorelease];
  [popupButton_ setAutoresizingMask:NSViewWidthSizable];
  [popupButton_ setBordered:NO];
  [popupButton_ setBezelStyle:NSTexturedRoundedBezelStyle];
  [popupButton_ setPullsDown:YES];
  // TODO(pinkerton): this doesn't work, not sure why.
  [popupButton_ setPreferredEdge:NSMaxYEdge];
  // TODO(pinkerton): no matter what, the arrows always draw in the middle
  // of the button. We can turn off the arrows entirely, but then will the
  // user ever know to click it? Leave them on for now.
  //[[popupButton_ cell] setArrowPosition:NSPopUpNoArrow];
  [[popupButton_ cell] setAltersStateOfSelectedItem:NO];
  // If we don't add this, no title will ever display.
  [popupButton_ addItemWithTitle:@"placeholder"];
  [view_ addSubview:popupButton_];

  // Register for notifications that the menu is about to display so we can
  // fill it in lazily
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(showMenu:)
             name:NSPopUpButtonCellWillPopUpNotification
           object:nil];

  // Create the close box and position at the left of the view.
  NSRect closeFrame = NSMakeRect(startFrame.size.width - kCloseBoxSize,
                                 kCloseBoxPaddingY,
                                 kCloseBoxSize,
                                 kCloseBoxSize);
  NSButton* close = [[[NSButton alloc] initWithFrame:closeFrame] autorelease];
  [close setAutoresizingMask:NSViewMinXMargin];
  [close setImage:nsimage_cache::ImageNamed(@"close_bar.pdf")];
  [close setAlternateImage:nsimage_cache::ImageNamed(@"close_bar_p.pdf")];
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
// TODO(pinkerton): fix this so that it measures the text so that it can
// be localized.
#if 0
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
        NSFontAttributeName, [NSFont systemFontOfSize:25],
        nil];
  NSSize stringSize = [label sizeWithAttributes:attributes];
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
  [popupButton_ setTitle:label];
}

// Called when the user selects an item from the popup menu. The tag, if below
// |kImpossibleNumberOfPopups| will be the index into the container's popup
// array. In that case, we should display the popup. If >=
// |kImpossibleNumberOfPopups|, it represents a host that we should whitelist.
// |sender| is the NSMenuItem that was chosen.
- (void)menuAction:(id)sender {
  size_t tag = static_cast<size_t>([sender tag]);
  if (tag < BlockedPopupContainer::kImpossibleNumberOfPopups) {
    container_->LaunchPopupAtIndex(tag);
  } else {
    size_t hostIndex = tag - BlockedPopupContainer::kImpossibleNumberOfPopups;
    container_->ToggleWhitelistingForHost(hostIndex);
  }
}

namespace {
void GetURLAndTitleForPopup(
    const BlockedPopupContainer* container,
    size_t index,
    string16* url,
    string16* title) {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = container->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToUTF16(tab_contents_url.possibly_invalid_spec());
  *title = tab_contents->GetTitle();
}
}  // namespace

// Build a new popup menu from scratch. The menu contains the blocked popups
// (tags being the popup's index), followed by the list of hosts from which
// the popups were blocked (tags being |kImpossibleNumberOfPopups| + host
// index). The hosts are used to toggle whitelisting for a site.
- (NSMenu*)buildMenu {
  NSMenu* menu = [[[NSMenu alloc] init] autorelease];

  // For pop-down menus, the first item is what is displayed while tracking the
  // menu and it remains there if nothing is selected. Set it to the
  // current title.
  NSString* currentTitle = [popupButton_ title];
  scoped_nsobject<NSMenuItem> dummy(
      [[NSMenuItem alloc] initWithTitle:currentTitle
                                 action:nil
                          keyEquivalent:@""]);
  [menu addItem:dummy.get()];

  // Add the list of blocked popups titles to the menu. We set the array index
  // as the tag for use in the menu action rather than relying on the menu item
  // index.
  const size_t count = container_->GetBlockedPopupCount();
  for (size_t i = 0; i < count; ++i) {
    string16 url, title;
    GetURLAndTitleForPopup(container_, i, &url, &title);
    NSString* titleStr = base::SysUTF16ToNSString(
        l10n_util::GetStringFUTF16(IDS_POPUP_TITLE_FORMAT, url, title));
    scoped_nsobject<NSMenuItem> item(
        [[NSMenuItem alloc] initWithTitle:titleStr
                                   action:@selector(menuAction:)
                            keyEquivalent:@""]);
    [item setTag:i];
    [item setTarget:self];
    [menu addItem:item.get()];
  }

  // Add the list of hosts. We begin tagging these at
  // |kImpossibleNumberOfPopups|. If whitelisting has already been enabled
  // for a site, mark it with a checkmark.
  std::vector<std::string> hosts(container_->GetHosts());
  if (!hosts.empty() && count)
    [menu addItem:[NSMenuItem separatorItem]];
  for (size_t i = 0; i < hosts.size(); ++i) {
    NSString* titleStr = base::SysUTF8ToNSString(
        l10n_util::GetStringFUTF8(IDS_POPUP_HOST_FORMAT,
                                  UTF8ToUTF16(hosts[i])));
    scoped_nsobject<NSMenuItem> item(
        [[NSMenuItem alloc] initWithTitle:titleStr
                                   action:@selector(menuAction:)
                            keyEquivalent:@""]);
    if (container_->IsHostWhitelisted(i))
      [item setState:NSOnState];
    [item setTag:BlockedPopupContainer::kImpossibleNumberOfPopups + i];
    [item setTarget:self];
    [menu addItem:item.get()];
  }

  return menu;
}

// Called when the popup button is about to display the menu, giving us a
// chance to fill in the contents.
- (void)showMenu:(NSNotification*)notify {
  NSMenu* menu = [self buildMenu];
  [[notify object] setMenu:menu];
}

- (NSView*)view {
  return view_.get();
}

- (NSPopUpButton*)popupButton {
  return popupButton_;
}

// Only used for testing.
- (void)setContainer:(BlockedPopupContainer*)container {
  container_ = container;
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
