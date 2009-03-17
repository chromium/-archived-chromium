// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/repost_form_warning_view.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/window/window.h"
#include "grit/generated_resources.h"

// Implementation of function declared in
// browser/tab_contents/repost_form_warning.h
void RunRepostFormWarningDialog(NavigationController* navigation_controller) {
  RepostFormWarningView* dialog =
      new RepostFormWarningView(navigation_controller);
}

RepostFormWarningView::RepostFormWarningView(
    NavigationController* navigation_controller)
      : navigation_controller_(navigation_controller),
        message_box_view_(NULL) {
  message_box_view_ = new MessageBoxView(
      MessageBoxView::kIsConfirmMessageBox,
      l10n_util::GetString(IDS_HTTP_POST_WARNING),
      L"");
  // TODO(beng): fix this - this dialog box should be shown by a method on the
  //             Browser.
  HWND root_hwnd = NULL;
  if (BrowserList::GetLastActive()) {
    root_hwnd = reinterpret_cast<HWND>(BrowserList::GetLastActive()->
        window()->GetNativeHandle());
  }
  views::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this)->Show();
  NotificationService::current()->AddObserver(
      this, NotificationType::LOAD_START, NotificationService::AllSources());
  NotificationService::current()->AddObserver(
      this, NotificationType::TAB_CLOSING, NotificationService::AllSources());
}

RepostFormWarningView::~RepostFormWarningView() {
  NotificationService::current()->RemoveObserver(
      this, NotificationType::LOAD_START, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NotificationType::TAB_CLOSING, NotificationService::AllSources());
}

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::DialogDelegate implementation:

std::wstring RepostFormWarningView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_HTTP_POST_WARNING_TITLE);
}

std::wstring RepostFormWarningView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DialogDelegate::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_HTTP_POST_WARNING_RESEND);
  if (button == DialogDelegate::DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_HTTP_POST_WARNING_CANCEL);
  return L"";
}

void RepostFormWarningView::DeleteDelegate() {
  delete this;
}

bool RepostFormWarningView::Cancel() {
  return true;
}

bool RepostFormWarningView::Accept() {
  if (navigation_controller_)
    navigation_controller_->Reload(false);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::WindowDelegate implementation:

views::View* RepostFormWarningView::GetContentsView() {
  return message_box_view_;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, private:

void RepostFormWarningView::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (window() && navigation_controller_ &&
      (type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING) &&
      Source<NavigationController>(source).ptr() == navigation_controller_) {
    navigation_controller_ = NULL;
    window()->Close();
  }
}
