// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__

namespace ChromeView {
class KeyEvent;
class View;
}
class RenderViewHost;
class TabContents;

#include "chrome/common/notification_observer.h"
#include "chrome/views/controls/hwnd_view.h"
#include "chrome/views/focus/focus_manager.h"

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

  // Overridden from HWNDView.
  virtual views::FocusTraversable* GetFocusTraversableParent();
  virtual views::View* GetFocusTraversableParentView();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

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

  // The current TabContents shown.
  TabContents* tab_contents_;

  DISALLOW_EVIL_CONSTRUCTORS(TabContentsContainerView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__
