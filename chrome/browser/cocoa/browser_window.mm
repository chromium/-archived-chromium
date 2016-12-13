// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/browser_window.h"

#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"

@implementation ChromeBrowserWindow

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  // Extract info from |event|.
  NSUInteger modifers = [event modifierFlags];
  const bool cmdKey = modifers & NSCommandKeyMask;
  const bool shiftKey = modifers & NSShiftKeyMask;
  const bool cntrlKey = modifers & NSControlKeyMask;
  const int keyCode = [event keyCode];

  int cmdNum = CommandForKeyboardShortcut(cmdKey, shiftKey, cntrlKey,
      keyCode);

  BrowserWindowController* controller =
      (BrowserWindowController*)[self delegate];
  // A bit of sanity.
  DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
  DCHECK([controller respondsToSelector:@selector(executeCommand:)]);

  if (cmdNum != -1) {
    [controller executeCommand:cmdNum];
    return YES;
  }

  return [super performKeyEquivalent:event];
}

@end
