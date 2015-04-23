// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_RWHVM_EDITCOMMAND_HELPER_H_
#define CHROME_BROWSER_COCOA_RWHVM_EDITCOMMAND_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// RenderWidgetHostViewMacEditCommandHelper is the real name of this class
// but that's too long, so we use a shorter version.
//
// This class mimics the behavior of WebKit's WebView class in a way that makes
// sense for Chrome.
//
// WebCore has the concept of "core commands", basically named actions such as
// "Select All" and "Move Cursor Left".  The commands are executed using their
// string value by WebCore.
//
// This class is responsible for 2 things:
// 1. Provide an abstraction to determine the enabled/disabled state of menu
// items that correspond to edit commands.
// 2. Hook up a bunch of objc selectors to the RenderWidgetHostViewCocoa object.
// (note that this is not a misspelling of RenderWidgetHostViewMac, it's in
//  fact a distinct object) When these selectors are called, the relevant
// edit command is executed in WebCore.
class RWHVMEditCommandHelper {
   FRIEND_TEST(RWHVMEditCommandHelperTest, TestAddEditingSelectorsToClass);
   FRIEND_TEST(RWHVMEditCommandHelperTest, TestEditingCommandDelivery);

 public:
  RWHVMEditCommandHelper();

  // Adds editing selectors to the objc class using the objc runtime APIs.
  // Each selector is connected to a single c method which forwards the message
  // to WebCore's ExecuteEditCommand() function.
  // This method is idempotent.
  // The class passed in must conform to the RenderWidgetHostViewMacOwner
  // protocol.
  void AddEditingSelectorsToClass(Class klass);

  // Is a given menu item currently enabled?
  // SEL - the objc selector currently associated with an NSMenuItem.
  // owner - An object we can retrieve a RenderWidgetHostViewMac from to
  // determine the command states.
  bool IsMenuItemEnabled(SEL item_action,
                         id<RenderWidgetHostViewMacOwner> owner);

 protected:
  // Gets a list of all the selectors that AddEditingSelectorsToClass adds to
  // the aforementioned class.
  // returns an array of NSStrings WITHOUT the trailing ':'s.
  NSArray* GetEditSelectorNames();

 private:
  base::hash_set<std::string> edit_command_set_;
  DISALLOW_COPY_AND_ASSIGN(RWHVMEditCommandHelper);
};

#endif //  CHROME_BROWSER_COCOA_RWHVM_EDITCOMMAND_HELPER_H_
