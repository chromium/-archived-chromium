// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_

// This file contains functions for running a variety of browser dialogs and
// popups. The dialogs here are the ones that the caller does not need to
// access the class of the popup. It allows us to break dependencies by
// allowing the callers to not depend on the classes implementing the dialogs.

class Profile;
class TabContents;

namespace views {
class Widget;
}  // namespace views

// Shows the "Report a problem with this page" dialog box. See BugReportView.
void ShowBugReportView(views::Widget* parent,
                       Profile* profile,
                       TabContents* tab);

#endif  // CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_
