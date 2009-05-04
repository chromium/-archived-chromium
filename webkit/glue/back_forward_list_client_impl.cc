// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"

#include "HistoryItem.h"
#undef LOG

#include "webkit/glue/back_forward_list_client_impl.h"
#include "webkit/glue/webview_impl.h"

namespace webkit_glue {

const char kBackForwardNavigationScheme[] = "chrome-back-forward";

BackForwardListClientImpl::BackForwardListClientImpl(WebViewImpl* webview)
    : webview_(webview) {
}

BackForwardListClientImpl::~BackForwardListClientImpl() {
}

void BackForwardListClientImpl::SetCurrentHistoryItem(
    WebCore::HistoryItem* item) {
  previous_item_ = current_item_;
  current_item_ = item;
}

WebCore::HistoryItem* BackForwardListClientImpl::GetPreviousHistoryItem()
    const {
  return previous_item_.get();
}

void BackForwardListClientImpl::addItem(PassRefPtr<WebCore::HistoryItem> item) {
  previous_item_ = current_item_;
  current_item_ = item;

  // If WebCore adds a new HistoryItem, it means this is a new navigation (ie,
  // not a reload or back/forward).
  webview_->ObserveNewNavigation();

  if (webview_->delegate())
    webview_->delegate()->DidAddHistoryItem();
}

void BackForwardListClientImpl::goToItem(WebCore::HistoryItem* item) {
  previous_item_ = current_item_;
  current_item_ = item;

  if (pending_history_item_ == item)
    pending_history_item_ = NULL;
}

WebCore::HistoryItem* BackForwardListClientImpl::currentItem() {
  return current_item_.get();
}

WebCore::HistoryItem* BackForwardListClientImpl::itemAtIndex(int index) {
  if (!webview_->delegate())
    return NULL;

  // Since we don't keep the entire back/forward list, we have no way to
  // properly implement this method.  We return a dummy entry instead that we
  // intercept in our FrameLoaderClient implementation in case WebCore asks
  // to navigate to this HistoryItem.

  // TODO(darin): We should change WebCore to handle history.{back,forward,go}
  // differently.  It should perhaps just ask the FrameLoaderClient to perform
  // those navigations.

  WebCore::String url_string = WebCore::String::format(
      "%s://go/%d", kBackForwardNavigationScheme, index);

  pending_history_item_ =
      WebCore::HistoryItem::create(url_string, WebCore::String(), 0.0);
  return pending_history_item_.get();
}

int BackForwardListClientImpl::backListCount() {
  if (!webview_->delegate())
    return 0;

  return webview_->delegate()->GetHistoryBackListCount();
}

int BackForwardListClientImpl::forwardListCount() {
  if (!webview_->delegate())
    return 0;

  return webview_->delegate()->GetHistoryForwardListCount();
}

void BackForwardListClientImpl::close() {
  current_item_ = NULL;
  previous_item_ = NULL;
  pending_history_item_ = NULL;
}

} // namespace webkit_glue
