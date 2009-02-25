// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/accelerator_handler.h"
#include "googleurl/src/gurl.h"

namespace ui_test_utils {

namespace {

// Used to block until a navigation completes.
class NavigationNotificationObserver : public NotificationObserver {
 public:
  explicit NavigationNotificationObserver(NavigationController* controller)
      : navigation_started_(false) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_START,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
    RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::NAV_ENTRY_COMMITTED ||
        type == NotificationType::LOAD_START) {
      navigation_started_ = true;
    } else if (type == NotificationType::LOAD_STOP) {
      if (navigation_started_) {
        navigation_started_ = false;
        MessageLoopForUI::current()->Quit();
      }
    }
  }

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

}  // namespace 

void RunMessageLoop() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
  views::AcceleratorHandler handler;
  loop->Run(&handler);
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

void WaitForNavigation(NavigationController* controller) {
  NavigationNotificationObserver observer(controller);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  NavigationController* controller =
      browser->GetSelectedTabContents()->controller();
  browser->OpenURLFromTab(browser->GetSelectedTabContents(), url, GURL(),
                          CURRENT_TAB, PageTransition::TYPED);
  WaitForNavigation(controller);
}

}  // namespace ui_test_utils
