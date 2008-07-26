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

#include "chrome/browser/repost_form_warning_dialog.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/navigation_controller.h"
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
// RepostFormWarningDialog, ChromeViews::DialogDelegate implementation:

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
  dialog_ = NULL;
  return true;
}

bool RepostFormWarningDialog::Accept() {
  dialog_ = NULL;
  if (navigation_controller_)
    navigation_controller_->ReloadDontCheckForRepost();
  return true;
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
  HWND root_hwnd = NULL;
  if (BrowserList::GetLastActive())
    root_hwnd = BrowserList::GetLastActive()->GetTopLevelHWND();
  ChromeViews::Window* dialog_ =
      ChromeViews::Window::CreateChromeWindow(root_hwnd, gfx::Rect(),
                                              message_box_view_, this);
  dialog_->Show();
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
  if (dialog_ && navigation_controller_ &&
      (type == NOTIFY_LOAD_START || type == NOTIFY_TAB_CLOSING) &&
      Source<NavigationController>(source).ptr() == navigation_controller_) {
      navigation_controller_ = NULL;
      dialog_->Close();
    }
}
