// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H_
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H_

namespace ChromeView {
class KeyEvent;
class View;
}
class RenderViewHost;
class TabContents;

#include "chrome/common/notification_registrar.h"
#include "views/controls/hwnd_view.h"
#include "views/focus/focus_manager.h"

// This View contains the TabContents.
// It takes care of linking the TabContents to the browser RootView so that the
// focus can traverse from one to the other when pressing Tab/Shift-Tab.
class TabContentsContainerView : public views::HWNDView,
                                 public NotificationObserver {
 public:
  TabContentsContainerView();
  virtual ~TabContentsContainerView();

  // Make the specified tab visible.
  void SetTabContents(TabContents* tab_contents);
  TabContents* GetTabContents() const { return tab_contents_; }

  // Overridden from View.
  virtual views::FocusTraversable* GetFocusTraversable();
  virtual bool IsFocusable() const;
  virtual void Focus();
  virtual void RequestFocus();
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse);
  virtual bool CanProcessTabKeyEvents();
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);

  // Overridden from HWNDView.
  virtual views::FocusTraversable* GetFocusTraversableParent();
  virtual views::View* GetFocusTraversableParentView();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Web content should be given first crack at accelerators. This function
  // returns false if the current tab is a webcontents.
  virtual bool ShouldLookupAccelerators(const views::KeyEvent& e);

 private:
  // Add or remove observers for events that we care about.
  void AddObservers();
  void RemoveObservers();

  // Called when the RenderViewHost of the hosted TabContents has changed, e.g.
  // to show an interstitial page.
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host);

  // Called when a TabContents is destroyed. This gives us a chance to clean
  // up our internal state if the TabContents is somehow destroyed before we
  // get notified.
  void TabContentsDestroyed(TabContents* contents);

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  // The current TabContents shown.
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainerView);
};

#endif  // CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H_
