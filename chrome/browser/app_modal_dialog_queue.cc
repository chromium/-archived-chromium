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

#include "chrome/browser/app_modal_dialog_queue.h"

#include "chrome/browser/browser_list.h"

// static
std::queue<ChromeViews::AppModalDialogDelegate*>*
    AppModalDialogQueue::app_modal_dialog_queue_ = NULL;

// static
void AppModalDialogQueue::AddDialog(
    ChromeViews::AppModalDialogDelegate* dialog) {
  DCHECK(dialog->IsModal());
  if (!app_modal_dialog_queue_) {
    app_modal_dialog_queue_ =
        new std::queue<ChromeViews::AppModalDialogDelegate*>;
    ShowModalDialog(dialog);
  }

  app_modal_dialog_queue_->push(dialog);
}

// static
void AppModalDialogQueue::ShowNextDialog() {
  app_modal_dialog_queue_->pop();
  BrowserList::SetIsShowingAppModalDialog(false);
  if (!app_modal_dialog_queue_->empty()) {
    ShowModalDialog(app_modal_dialog_queue_->front());
  } else {
    delete app_modal_dialog_queue_;
    app_modal_dialog_queue_ = NULL;
  }
}

// static
void AppModalDialogQueue::ActivateModalDialog() {
  if (!app_modal_dialog_queue_->empty())
    app_modal_dialog_queue_->front()->ActivateModalDialog();
}

// static
void AppModalDialogQueue::ShowModalDialog(
    ChromeViews::AppModalDialogDelegate* dialog) {
  dialog->ShowModalDialog();
  BrowserList::SetIsShowingAppModalDialog(true);
}
