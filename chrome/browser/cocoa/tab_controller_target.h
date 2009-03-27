// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_CONTROLLER_TARGET_H_
#define CHROME_BROWSER_COCOA_TAB_CONTROLLER_TARGET_H_

// A protocol to be implemented by a TabController's target.
@protocol TabControllerTarget
- (void)selectTab:(id)sender;
- (void)closeTab:(id)sender;
@end

#endif  // CHROME_BROWSER_COCOA_TAB_CONTROLLER_TARGET_H_
