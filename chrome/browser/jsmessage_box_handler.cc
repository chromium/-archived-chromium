// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jsmessage_box_handler.h"

#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxHandler, public:

// static
void JavascriptMessageBoxHandler::RunJavascriptMessageBox(
    WebContents* web_contents,
    int dialog_flags,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox,
    IPC::Message* reply_msg) {
  JavascriptMessageBoxHandler* handler =
      new JavascriptMessageBoxHandler(web_contents, dialog_flags,
                                      message_text, default_prompt_text,
                                      display_suppress_checkbox, reply_msg);
  AppModalDialogQueue::AddDialog(handler);
}

JavascriptMessageBoxHandler::~JavascriptMessageBoxHandler() {
}

//////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxHandler, views::DialogDelegate implementation:

int JavascriptMessageBoxHandler::GetDialogButtons() const {
  int dialog_buttons = 0;
  if (dialog_flags_ & MessageBoxView::kFlagHasOKButton)
    dialog_buttons = DIALOGBUTTON_OK;

  if (dialog_flags_ & MessageBoxView::kFlagHasCancelButton)
    dialog_buttons |= DIALOGBUTTON_CANCEL;

  return dialog_buttons;
}

std::wstring JavascriptMessageBoxHandler::GetWindowTitle() const {
  if (!web_contents_)
    return std::wstring();

  GURL url = web_contents_->GetURL();
  if (!url.has_host())
    return l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);

  // We really only want the scheme, hostname, and port.
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL clean_url = url.ReplaceComponents(replacements);

  // TODO(brettw) it should be easier than this to do the correct language
  // handling without getting the accept language from the profil.
  std::wstring base_address = gfx::ElideUrl(clean_url, ChromeFont(), 0,
      web_contents_->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  return l10n_util::GetStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, base_address);
}

void JavascriptMessageBoxHandler::WindowClosing() {
  dialog_ = NULL;

  if (message_box_view_->IsCheckBoxSelected() && web_contents_)
    web_contents_->set_suppress_javascript_messages(true);

  delete this;
}

bool JavascriptMessageBoxHandler::Cancel() {
  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive it's activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  AppModalDialogQueue::ShowNextDialog();

  if (web_contents_) {
    web_contents_->OnJavaScriptMessageBoxClosed(reply_msg_, false,
                                                EmptyWString());
  }
  return true;
}

bool JavascriptMessageBoxHandler::Accept() {
  AppModalDialogQueue::ShowNextDialog();

  if (web_contents_) {
    web_contents_->OnJavaScriptMessageBoxClosed(
        reply_msg_, true, message_box_view_->GetInputText());
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxHandler, views::AppModalDialogDelegate
// implementation:

void JavascriptMessageBoxHandler::ShowModalDialog() {
  // If the WebContents that created this dialog navigated away before this
  // dialog became visible, simply show the next dialog if any.
  if (!web_contents_) {
    AppModalDialogQueue::ShowNextDialog();
    delete this;
    return;
  }

  web_contents_->Activate();
  HWND root_hwnd = GetAncestor(web_contents_->GetContainerHWND(), GA_ROOT);
  dialog_ = views::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this);
  dialog_->Show();
}

void JavascriptMessageBoxHandler::ActivateModalDialog() {
  // Ensure that the dialog is visible and at the top of the z-order. These
  // conditions may not be true if the dialog was opened on a different virtual
  // desktop to the one the browser window is on.
  dialog_->Show();
  dialog_->Activate();
}

///////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxHandler, views::WindowDelegate implementation:

views::View* JavascriptMessageBoxHandler::GetContentsView() {
  return message_box_view_;
}

views::View* JavascriptMessageBoxHandler::GetInitiallyFocusedView() const {
  if (message_box_view_->text_box())
    return message_box_view_->text_box();
  return views::AppModalDialogDelegate::GetInitiallyFocusedView();
}

///////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxHandler, private:

void JavascriptMessageBoxHandler::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  bool web_contents_gone = false;
  if (!web_contents_)
    return;

  if (type == NOTIFY_NAV_ENTRY_COMMITTED &&
      Source<NavigationController>(source).ptr() == web_contents_->controller())
    web_contents_gone = true;

  if (type == NOTIFY_TAB_CONTENTS_DESTROYED &&
      Source<TabContents>(source).ptr() ==
      static_cast<TabContents*>(web_contents_))
    web_contents_gone = true;

  if (web_contents_gone) {
    web_contents_ = NULL;

    // If the dialog is visible close it.
    if (dialog_)
      dialog_->Close();
  }
}

JavascriptMessageBoxHandler::JavascriptMessageBoxHandler(
    WebContents* web_contents,
    int dialog_flags,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox,
    IPC::Message* reply_msg)
      : web_contents_(web_contents),
        reply_msg_(reply_msg),
        dialog_flags_(dialog_flags),
        dialog_(NULL),
        message_box_view_(new MessageBoxView(dialog_flags, message_text,
                                             default_prompt_text)) {
  DCHECK(message_box_view_);
  DCHECK(reply_msg_);

  if (display_suppress_checkbox) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }

  // Make sure we get navigation notifications so we know when our parent
  // contents will disappear or navigate to a different page.
  registrar_.Add(this, NOTIFY_NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
  registrar_.Add(this, NOTIFY_TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
}
