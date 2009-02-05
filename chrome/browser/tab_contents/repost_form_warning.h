// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_REPOST_FORM_WARNING_H_
#define CHROME_BROWSER_TAB_CONTENTS_REPOST_FORM_WARNING_H_

class NavigationController;

// Runs the form repost warning dialog. If the user accepts the action, then
// it will call Reload on the navigation controller back with check_for_repost
// set to false.
//
// This function is implemented by the platform-specific frontend.
void RunRepostFormWarningDialog(NavigationController* nav_controller);

#endif  // CHROME_BROWSER_TAB_CONTENTS_REPOST_FORM_WARNING_H_
