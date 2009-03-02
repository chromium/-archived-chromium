// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
#define CHROME_BROWSER_GTK_TAB_CONTENTS_CONTAINER_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/common/notification_observer.h"

class RenderViewHost;
class TabContents;

class TabContentsContainerGtk : public NotificationObserver {
 public:
  TabContentsContainerGtk();
  ~TabContentsContainerGtk();

  // Inserts our GtkWidget* hierarchy into a GtkBox managed by our owner.
  void AddContainerToBox(GtkWidget* widget);

  // Make the specified tab visible.
  void SetTabContents(TabContents* tab_contents);
  TabContents* GetTabContents() const { return tab_contents_; }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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

  // The currently visible TabContents.
  TabContents* tab_contents_;

  // We keep a GtkVBox which is inserted into this object's owner's GtkWidget
  // hierarchy. We then insert and remove WebContents GtkWidgets into this
  // vbox_.
  GtkWidget* vbox_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainerGtk);
};

#endif  // CHROME_BROWSER_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
