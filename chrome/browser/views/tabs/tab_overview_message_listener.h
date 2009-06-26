// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_MESSAGE_LISTENER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_MESSAGE_LISTENER_H_

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"

class Browser;
class NewBrowserWindowWidget;
class TabOverviewController;

// TabOverviewMessageListener listens for messages, showing/hiding the tab
// overview as necessary. TabOverviewMessageListener is created early on by
// browser init and only one instance ever exists.
class TabOverviewMessageListener : public MessageLoopForUI::Observer {
 public:
  static TabOverviewMessageListener* instance();

  // MessageLoopForUI::Observer overrides.
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

 private:
  friend struct DefaultSingletonTraits<TabOverviewMessageListener>;

  TabOverviewMessageListener();
  ~TabOverviewMessageListener();

  // Invoked when a valid TabOverviewTypes::Message is found on the message
  // loop..
  void ProcessMessage(const TabOverviewTypes::Message& message,
                      GdkWindow* window);

  // Shows the tab overview for |browser|.
  void ShowOverview(Browser* browser, int horizontal_center);

  // Hids the tab overview.
  void HideOverview();

  // If non-null tab overview is showing.
  scoped_ptr<TabOverviewController> controller_;

  // Non-null while in tab-overview mode.
  scoped_ptr<NewBrowserWindowWidget> new_browser_window_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewMessageListener);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_MESSAGE_LISTENER_H_
