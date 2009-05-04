// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIND_BAR_CONTROLLER_H_
#define CHROME_BROWSER_FIND_BAR_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_observer.h"

class FindBar;
class TabContents;

class FindBarController : public NotificationObserver {
 public:
  // FindBar takes ownership of |find_bar_view|.
  explicit FindBarController(FindBar* find_bar);

  virtual ~FindBarController();

  // Shows the find bar. Any previous search string will again be visible.
  void Show();

  // Ends the current session.
  void EndFindSession();

  // Accessor for the attached TabContents.
  TabContents* tab_contents() const { return tab_contents_; }

  // Changes the TabContents that this FindBar is attached to. This occurs when
  // the user switches tabs in the Browser window. |contents| can be NULL.
  void ChangeTabContents(TabContents* contents);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  FindBar* find_bar() const { return find_bar_.get(); }

 private:
  scoped_ptr<FindBar> find_bar_;

  // The TabContents we are currently associated with.  Can be NULL.
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(FindBarController);
};

#endif  // CHROME_BROWSER_FIND_BAR_CONTROLLER_H_
