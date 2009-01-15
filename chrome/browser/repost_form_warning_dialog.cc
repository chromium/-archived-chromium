// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/repost_form_warning_dialog.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningDialog, public:

// static
void RepostFormWarningDialog::RunRepostFormWarningDialog(
    NavigationController* navigation_controller) {
  RepostFormWarningDialog* dialog =
      new RepostFormWarningDialog(navigation_controller);
}

RepostFormWarningDialog::~RepostFormWarningDialog() {
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_LOAD_START, NotificationService::AllSources());
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_TAB_CLOSING, NotificationService::AllSources());
}

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningDialog, views::DialogDelegate implementation:

std::wstring RepostFormWarningDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_HTTP_POST_WARNING_TITLE);
}

std::wstring RepostFormWarningDialog::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DialogDelegate::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_HTTP_POST_WARNING_RESEND);
  if (button == DialogDelegate::DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_HTTP_POST_WARNING_CANCEL);
  return L"";
}

void RepostFormWarningDialog::WindowClosing() {
  delete this;
}

bool RepostFormWarningDialog::Cancel() {
  return true;
}

bool RepostFormWarningDialog::Accept() {
  if (navigation_controller_)
    navigation_controller_->Reload(false);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningDialog, views::WindowDelegate implementation:

views::View* RepostFormWarningDialog::GetContentsView() {
  return message_box_view_;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningDialog, private:

RepostFormWarningDialog::RepostFormWarningDialog(
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
  NotificationService::current()->
      AddObserver(this, NOTIFY_LOAD_START, NotificationService::AllSources());
  NotificationService::current()->
      AddObserver(this, NOTIFY_TAB_CLOSING, NotificationService::AllSources());
}

void RepostFormWarningDialog::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (window() && navigation_controller_ &&
      (type == NOTIFY_LOAD_START || type == NOTIFY_TAB_CLOSING) &&
      Source<NavigationController>(source).ptr() == navigation_controller_) {
      navigation_controller_ = NULL;
      window()->Close();
    }
}

