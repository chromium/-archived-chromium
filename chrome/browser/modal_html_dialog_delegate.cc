// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/modal_html_dialog_delegate.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"

ModalHtmlDialogDelegate::ModalHtmlDialogDelegate(
    const GURL& url, int width, int height, const std::string& json_arguments,
    IPC::Message* sync_result, TabContents* contents)
    : contents_(contents),
      sync_response_(sync_result) {
  // Listen for when the TabContents or its renderer dies.
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                 Source<TabContents>(contents_));

  // This information is needed to show the dialog HTML content.
  params_.url = url;
  params_.height = height;
  params_.width = width;
  params_.json_input = json_arguments;
}

ModalHtmlDialogDelegate::~ModalHtmlDialogDelegate() {
}

void ModalHtmlDialogDelegate::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DISCONNECTED);
  DCHECK(Source<TabContents>(source).ptr() == contents_);
  registrar_.RemoveAll();
  contents_ = NULL;
}

bool ModalHtmlDialogDelegate::IsDialogModal() const {
  return true;
}

GURL ModalHtmlDialogDelegate::GetDialogContentURL() const {
  return params_.url;
}

void ModalHtmlDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->set_width(params_.width);
  size->set_height(params_.height);
}

std::string ModalHtmlDialogDelegate::GetDialogArgs() const {
  return params_.json_input;
}

void ModalHtmlDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  // Our TabContents may have died before this point.
  if (contents_ && contents_->render_view_host()) {
    contents_->render_view_host()->ModalHTMLDialogClosed(sync_response_,
                                                         json_retval);
  }

  // We are done with this request, so delete us.
  delete this;
}
