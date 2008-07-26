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

#include "chrome/browser/js_before_unload_handler.h"

#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/message_box_view.h"

#include "generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// JavascriptBeforeUnloadHandler, public:

// static
void JavascriptBeforeUnloadHandler::RunBeforeUnloadDialog(
    WebContents* web_contents,
    const std::wstring& message_text,
    IPC::Message* reply_msg) {
  std::wstring full_message =
    message_text + L"\n\n" +
    l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);
  JavascriptBeforeUnloadHandler* handler =
      new JavascriptBeforeUnloadHandler(web_contents, full_message, reply_msg);
  AppModalDialogQueue::AddDialog(handler);
}

//////////////////////////////////////////////////////////////////////////////
// JavascriptBeforeUnloadHandler, ChromeViews::DialogDelegate implementation:

std::wstring JavascriptBeforeUnloadHandler::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
}

std::wstring JavascriptBeforeUnloadHandler::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DialogDelegate::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
  } else if (button == DialogDelegate::DIALOGBUTTON_CANCEL) {
    return l10n_util::GetString(
        IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
  }
  return L"";
}

JavascriptBeforeUnloadHandler::JavascriptBeforeUnloadHandler(
    WebContents* web_contents,
    const std::wstring& message_text,
    IPC::Message* reply_msg)
        : JavascriptMessageBoxHandler(web_contents,
                                      MessageBoxView::kIsJavascriptConfirm,
                                      message_text,
                                      L"",
                                      false,
                                      reply_msg) {}
