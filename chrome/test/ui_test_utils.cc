// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/widget/accelerator_handler.h"
#include "googleurl/src/gurl.h"

namespace ui_test_utils {

namespace {

// Used to block until a navigation completes.
class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 int number_of_navigations)
      : navigation_started_(false),
        navigations_completed_(0),
        number_of_navigations_(number_of_navigations) {
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
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        MessageLoopForUI::current()->Quit();
      }
    }
  }

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

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
  WaitForNavigations(controller, 1);
}

void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations) {
  NavigationNotificationObserver observer(controller, number_of_navigations);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateToURLBlockUntilNavigationsComplete(browser, url, 1);
}

void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigationController* controller =
      &browser->GetSelectedTabContents()->controller();
  browser->OpenURLFromTab(browser->GetSelectedTabContents(), url, GURL(),
                          CURRENT_TAB, PageTransition::TYPED);
  WaitForNavigations(controller, number_of_navigations);
}

JavaScriptRunner::JavaScriptRunner(WebContents* web_contents,
                                   const std::wstring& frame_xpath,
                                   const std::wstring& jscript)
    : web_contents_(web_contents),
      frame_xpath_(frame_xpath),
      jscript_(jscript) {
  NotificationService::current()->
      AddObserver(this, NotificationType::DOM_OPERATION_RESPONSE,
      Source<WebContents>(web_contents));
}

void JavaScriptRunner::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  Details<DomOperationNotificationDetails> dom_op_details(details);
  result_ = dom_op_details->json();
  // The Jasonified response has quotes, remove them.
  if (result_.length() > 1 && result_[0] == '"')
    result_ = result_.substr(1, result_.length() - 2);

  NotificationService::current()->
      RemoveObserver(this, NotificationType::DOM_OPERATION_RESPONSE,
      Source<WebContents>(web_contents_));
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

std::string JavaScriptRunner::Run() {
  // The DOMAutomationController requires an automation ID, even though we are
  // not using it in this case.
  web_contents_->render_view_host()->ExecuteJavascriptInWebFrame(
      frame_xpath_, L"window.domAutomationController.setAutomationId(0);");

  web_contents_->render_view_host()->ExecuteJavascriptInWebFrame(frame_xpath_,
                                                                 jscript_);
  ui_test_utils::RunMessageLoop();
  return result_;
}

}  // namespace ui_test_utils
