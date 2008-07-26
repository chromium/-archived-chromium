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

#include "chrome/browser/modal_html_dialog_delegate.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/render_view_host.h"

ModalHtmlDialogDelegate::ModalHtmlDialogDelegate(
    const GURL& url, int width, int height, const std::string& json_arguments,
    IPC::Message* sync_result, WebContents* contents)
  : contents_(contents),
    sync_response_(sync_result) {
  // Listen for when the WebContents or its renderer dies.
  NotificationService::current()->
      AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
      Source<WebContents>(contents_));

  // This information is needed to show the dialog HTML content.
  params_.url = url;
  params_.height = height;
  params_.width = width;
  params_.json_input = json_arguments;
}

ModalHtmlDialogDelegate::~ModalHtmlDialogDelegate() {
  NotificationService::current()->
      RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
      Source<WebContents>(contents_));
}

void ModalHtmlDialogDelegate::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NOTIFY_WEB_CONTENTS_DISCONNECTED);
  DCHECK(Source<WebContents>(source).ptr() == contents_);
  contents_ = NULL;  // No longer safe to access.
}

bool ModalHtmlDialogDelegate::IsModal() const {
  return true;
}

GURL ModalHtmlDialogDelegate::GetDialogContentURL() const {
  return params_.url;
}

void ModalHtmlDialogDelegate::GetDialogSize(CSize* size) const {
  size->cx = params_.width;
  size->cy = params_.height;
}

std::string ModalHtmlDialogDelegate::GetDialogArgs() const {
  return params_.json_input;
}

void ModalHtmlDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  // Our WebContents may have died before this point.
  if (contents_ && contents_->render_view_host()) {
    contents_->render_view_host()->ModalHTMLDialogClosed(sync_response_,
                                                         json_retval);
  }

  // We are done with this request, so delete us.
  delete this;
}
