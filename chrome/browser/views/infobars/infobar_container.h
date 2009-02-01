// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_

#include "chrome/common/notification_observer.h"
#include "chrome/views/view.h"

class BrowserView;
class InfoBarDelegate;
class TabContents;

// A views::View subclass that contains a collection of InfoBars associated with
// a TabContents.
class InfoBarContainer : public views::View,
                         public NotificationObserver {
 public:
  explicit InfoBarContainer(BrowserView* browser_view);
  virtual ~InfoBarContainer();

  // Changes the TabContents for which this container is showing InfoBars. Can
  // be NULL.
  void ChangeTabContents(TabContents* contents);

  // Called by child InfoBars as they animate. If |completed| is true, the
  // animation has finished running.
  void InfoBarAnimated(bool completed);

  // Remove the specified InfoBarDelegate from the selected TabContents. This
  // will notify us back and cause us to close the View. This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
 protected:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
 
 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Constructs the InfoBars needed to reflect the state of the current
  // TabContents associated with this container. No animations are run during
  // this process.
  void UpdateInfoBars();

  // Adds an InfoBar for the specified delegate, in response to a notification
  // from the selected TabContents. The InfoBar's appearance will be animated.
  void AddInfoBar(InfoBarDelegate* delegate);

  // Removes an InfoBar for the specified delegate, in response to a
  // notification from the selected TabContents. The InfoBar's disappearance
  // will be animated.
  void RemoveInfoBar(InfoBarDelegate* delegate);

  // The BrowserView that hosts this InfoBarContainer.
  BrowserView* browser_view_;

  // The TabContents for which we are currently showing InfoBars.
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
