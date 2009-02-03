// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"

#include "HistoryItem.h"
#undef LOG

#include "webkit/glue/back_forward_list_client_impl.h"
#include "webkit/glue/webhistoryitem_impl.h"
#include "webkit/glue/webview_impl.h"

namespace webkit_glue {

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

  if (pending_history_item_) {
    if (item == pending_history_item_->GetHistoryItem()) {
      // Let the main frame know this HistoryItem is loading, so it can cache
      // any ExtraData when the DataSource is created.
      webview_->main_frame()->set_currently_loading_history_item(
          pending_history_item_);
      pending_history_item_ = NULL;
    }
  }
}

WebCore::HistoryItem* BackForwardListClientImpl::currentItem() {
  return current_item_.get();
}

WebCore::HistoryItem* BackForwardListClientImpl::itemAtIndex(int index) {
  if (!webview_->delegate())
    return NULL;

  WebHistoryItem* item = webview_->delegate()->GetHistoryEntryAtOffset(index);
  if (!item)
    return NULL;

  // If someone has asked for a history item, we probably want to navigate to
  // it soon.  Keep track of it until goToItem is called.
  pending_history_item_ = static_cast<WebHistoryItemImpl*>(item);
  return pending_history_item_->GetHistoryItem();
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
