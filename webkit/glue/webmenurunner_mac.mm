// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmenurunner_mac.h"

#include "base/sys_string_conversions.h"

@interface WebMenuRunner (PrivateAPI)

// Worker function used during initialization.
- (void)addItem:(const WebMenuItem&)item;

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(id)sender;

@end  // WebMenuRunner (PrivateAPI)

@implementation WebMenuRunner

- (id)initWithItems:(const std::vector<WebMenuItem>&)items {
  if ((self = [super init])) {
    menu_.reset([[NSMenu alloc] initWithTitle:@""]);
    [menu_ setAutoenablesItems:NO];
    index_ = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
      [self addItem:items[i]];
  }
  return self;
}

- (void)addItem:(const WebMenuItem&)item {
  if (item.type == WebMenuItem::SEPARATOR) {
    [menu_ addItem:[NSMenuItem separatorItem]];
    return;
  }

  NSString* title = base::SysUTF16ToNSString(item.label);
  NSMenuItem* menuItem = [menu_ addItemWithTitle:title
                                          action:@selector(menuItemSelected:)
                                   keyEquivalent:@""];
  [menuItem setEnabled:(item.enabled && item.type != WebMenuItem::GROUP)];
  [menuItem setTarget:self];
}

// Reflects the result of the user's interaction with the popup menu. If NO, the
// menu was dismissed without the user choosing an item, which can happen if the
// user clicked outside the menu region or hit the escape key. If YES, the user
// selected an item from the menu.
- (BOOL)menuItemWasChosen {
  return menuItemWasChosen_;
}

- (void)menuItemSelected:(id)sender {
  menuItemWasChosen_ = YES;
}

- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index {
  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* button = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                            pullsDown:NO];
  [button autorelease];
  [button setMenu:menu_];
  [button selectItemAtIndex:index];

  // Display the menu, and set a flag if a menu item was chosen.
  [button performClickWithFrame:bounds inView:view];

  if ([self menuItemWasChosen])
    index_ = [button indexOfSelectedItem];
}

- (int)indexOfSelectedItem {
  return index_;
}

@end  // WebMenuRunner

namespace webkit_glue {

// Helper function for manufacturing input events to send to WebKit.
NSEvent* EventWithMenuAction(BOOL item_chosen, int window_num,
                             int item_height, int selected_index,
                             NSRect menu_bounds, NSRect view_bounds) {
  NSEvent* event = nil;
  double event_time = (double)(AbsoluteToDuration(UpTime())) / 1000.0;

  if (item_chosen) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    NSPoint click_pos;
    click_pos.x = menu_bounds.size.width / 2;

    // This is going to be hard to calculate since the button is painted by
    // WebKit, the menu by Cocoa, and we have to translate the selected_item
    // index to a coordinate that WebKit's PopupMenu expects which uses a
    // different font *and* expects to draw the menu below the button like we do
    // on Windows.
    // The WebKit popup menu thinks it will draw just below the button, so
    // create the click at the offset based on the selected item's index and
    // account for the different coordinate system used by NSView.
    int item_offset = selected_index * item_height + item_height / 2;
    click_pos.y = view_bounds.size.height - item_offset;
    event = [NSEvent mouseEventWithType:NSLeftMouseUp
                               location:click_pos
                          modifierFlags:0
                              timestamp:event_time
                           windowNumber:window_num
                                context:nil
                            eventNumber:0
                             clickCount:1
                               pressure:1.0];
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    NSPoint key_pos;
    key_pos.x = 0;
    key_pos.y = 0;
    NSString* escape_str = [NSString stringWithFormat:@"%c", 0x1B];
    event = [NSEvent keyEventWithType:NSKeyDown
                             location:key_pos
                        modifierFlags:0
                            timestamp:event_time
                         windowNumber:window_num
                              context:nil
                           characters:@""
          charactersIgnoringModifiers:escape_str
                            isARepeat:NO
                              keyCode:0x1B];
  }

  return event;
}

}  // namespace webkit_glue
