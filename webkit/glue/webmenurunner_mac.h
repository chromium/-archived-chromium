// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBMENURUNNER_MAC_H_
#define WEBKIT_GLUE_WEBMENURUNNER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "webkit/glue/webwidget_delegate.h"


// WebMenuRunner ---------------------------------------------------------------
// A class for determining whether an item was selected from an HTML select
// control, or if the menu was dismissed without making a selection. If a menu
// item is selected, MenuDelegate is informed and sets a flag which can be
// queried after the menu has finished running.

@interface WebMenuRunner : NSObject {
 @private
  // The actual menu object, which we own.
  NSMenu* menu_;

  // A flag set to YES if a menu item was chosen, or NO if the menu was
  // dismissed without selecting an item.
  BOOL menuItemWasChosen_;

  // The index of the selected menu item.
  int index_;
}

// Initializes the MenuDelegate with a list of items sent from WebKit.
- (id)initWithItems:(const std::vector<WebMenuItem>&)items;
- (void)dealloc;

// Worker function used during initialization.
- (void)addItem:(const WebMenuItem&)item;

// Returns YES if an item was selected from the menu, NO if the menu was
// dismissed.
- (BOOL)menuItemWasChosen;

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(id)sender;

// Displays and runs a native popup menu.
- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index;

// Returns the index of selected menu item, or its initial value (-1) if no item
// was selected.
- (int)indexOfSelectedItem;

@end  // @interface WebMenuRunner

// Helper function for manufacturing input events to send to WebKit. If
// |item_chosen| is YES, we manufacture a mouse click event that corresponds to
// the menu item that was selected, |selected_index|, based on the position of
// the mouse click. Of |item_chosen| is NO, we create a keyboard event that
// simulates an ESC (menu dismissal) action. The event is designed to be sent to
// WebKit for processing by the PopupMenu class.
NSEvent* CreateEventForMenuAction(BOOL item_chosen, int window_num,
                                  int item_height, int selected_index,
                                  NSRect menu_bounds, NSRect view_bounds);

#endif // WEBKIT_GLUE_WEBMENURUNNER_MAC_H_
