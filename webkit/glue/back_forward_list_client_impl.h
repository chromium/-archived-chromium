// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_BACK_FORWARD_LIST_CLIENT_IMPL_H_
#define WEBKIT_GLUE_BACK_FORWARD_LIST_CLIENT_IMPL_H_

#include "BackForwardList.h"

class WebViewImpl;

namespace webkit_glue {

extern const char kBackForwardNavigationScheme[];

class BackForwardListClientImpl : public WebCore::BackForwardListClient {
 public:
  BackForwardListClientImpl(WebViewImpl* webview);
  ~BackForwardListClientImpl();

  void SetCurrentHistoryItem(WebCore::HistoryItem* item);
  WebCore::HistoryItem* GetPreviousHistoryItem() const;

 private:
  // WebCore::BackForwardListClient methods:
  virtual void addItem(PassRefPtr<WebCore::HistoryItem> item);
  virtual void goToItem(WebCore::HistoryItem* item);
  virtual WebCore::HistoryItem* currentItem();
  virtual WebCore::HistoryItem* itemAtIndex(int index);
  virtual int backListCount();
  virtual int forwardListCount();
  virtual void close();

  WebViewImpl* webview_;

  RefPtr<WebCore::HistoryItem> previous_item_;
  RefPtr<WebCore::HistoryItem> current_item_;

  // The last history item that was accessed via itemAtIndex().  We keep track
  // of this until goToItem() is called, so we can track the navigation.
  RefPtr<WebCore::HistoryItem> pending_history_item_;
};

} // namespace webkit_glue

#endif  // WEBKIT_GLUE_BACK_FORWARD_LIST_CLIENT_IMPL_H_
