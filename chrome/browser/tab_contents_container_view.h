// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__
#define CHROME_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__

namespace ChromeView {
class KeyEvent;
class View;
}
class RenderViewHost;
class TabContents;

#include "chrome/common/notification_service.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/hwnd_view.h"

// This View contains the TabContents.
// It takes care of linking the TabContents to the browser RootView so that the
// focus can traverse from one to the other when pressing Tab/Shift-Tab.
class TabContentsContainerView : public ChromeViews::HWNDView,
                                 public NotificationObserver {
 public:
  TabContentsContainerView();
  virtual ~TabContentsContainerView();

  // Make the specified tab visible.
  void SetTabContents(TabContents* tab_contents);
  TabContents* GetTabContents() const { return tab_contents_; }

  // Overridden from View.
  virtual ChromeViews::FocusTraversable* GetFocusTraversable();
  virtual bool IsFocusable() const;
  virtual void Focus();
  virtual void RequestFocus();
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse);
  virtual bool CanProcessTabKeyEvents();

  // Overridden from HWNDView.
  virtual ChromeViews::FocusTraversable* GetFocusTraversableParent();
  virtual ChromeViews::View* GetFocusTraversableParentView();

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
  virtual bool ShouldLookupAccelerators(const ChromeViews::KeyEvent& e);

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

#endif  // #ifndef CHROME_VIEWS_TAB_CONTENTS_CONTAINER_VIEW_H__