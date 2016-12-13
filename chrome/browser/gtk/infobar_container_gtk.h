// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INFOBAR_CONTAINER_GTK_H_
#define CHROME_BROWSER_GTK_INFOBAR_CONTAINER_GTK_H_

#include "base/basictypes.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class BrowserWindow;
class InfoBarDelegate;
class TabContents;

typedef struct _GtkWidget GtkWidget;

class InfoBarContainerGtk : public NotificationObserver {
 public:
  explicit InfoBarContainerGtk(BrowserWindow* browser_window);
  virtual ~InfoBarContainerGtk();

  // Get the native widget.
  GtkWidget* widget() const { return container_.get(); }

  // Changes the TabContents for which this container is showing InfoBars. Can
  // be NULL, in which case we will simply detach ourselves from the old tab
  // contents.
  void ChangeTabContents(TabContents* contents);

  // Remove the specified InfoBarDelegate from the selected TabContents. This
  // will notify us back and cause us to close the View. This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Returns the total pixel height of all infobars in this container that
  // are currently closing.
  int TotalHeightOfClosingBars() const;

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
  // from the selected TabContents.
  void AddInfoBar(InfoBarDelegate* delegate, bool animated);

  // Removes an InfoBar for the specified delegate, in response to a
  // notification from the selected TabContents. The InfoBar's disappearance
  // will be animated.
  void RemoveInfoBar(InfoBarDelegate* delegate);

  NotificationRegistrar registrar_;

  // The BrowserView that hosts this InfoBarContainer.
  BrowserWindow* browser_window_;

  // The TabContents for which we are currently showing InfoBars.
  TabContents* tab_contents_;

  // VBox that holds the info bars.
  OwnedWidgetGtk container_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerGtk);
};

#endif  // CHROME_BROWSER_GTK_INFOBAR_CONTAINER_GTK_H_
