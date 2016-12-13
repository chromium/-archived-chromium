// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_

#include "chrome/browser/views/tab_contents/native_tab_contents_container.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class NativeTabContentsContainer;
class RenderViewHost;
class TabContents;

class TabContentsContainer : public views::View,
                             public NotificationObserver {
 public:
  TabContentsContainer();
  virtual ~TabContentsContainer();

  // Changes the TabContents associated with this view.
  void ChangeTabContents(TabContents* contents);

  View* GetFocusView() { return native_container_->GetView(); }

  // Accessor for |tab_contents_|.
  TabContents* tab_contents() const { return tab_contents_; }

  // Called by the BrowserView to notify that |tab_contents| got the focus.
  void TabContentsFocused(TabContents* tab_contents);

  // Tells the container to update less frequently during resizing operations
  // so performance is better.
  void SetFastResize(bool fast_resize);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

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

  // An instance of a NativeTabContentsContainer object that holds the native
  // view handle associated with the attached TabContents.
  NativeTabContentsContainer* native_container_;

  // The attached TabContents.
  TabContents* tab_contents_;

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainer);
};

#endif  // CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
