// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H_
#define WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H_

#include "webkit/glue/webhistoryitem.h"
#include "googleurl/src/gurl.h"

#include "RefPtr.h"

namespace WebCore {
  class HistoryItem;
}

class WebHistoryItemImpl : public WebHistoryItem {
 public:
  WebHistoryItemImpl(const GURL& url,
                     const std::wstring& title,
                     const std::string& history_state,
                     WebRequest::ExtraData* extra_data);
  virtual ~WebHistoryItemImpl();

  // WebHistoryItem
  virtual const GURL& GetURL() const;
  virtual const std::wstring& GetTitle() const;
  virtual const std::string& GetHistoryState() const;
  virtual WebRequest::ExtraData* GetExtraData() const;

  // WebHistoryItemImpl
  // Returns a WebCore::HistoryItem based on the history_state.  This is
  // lazily-created and cached.
  WebCore::HistoryItem* GetHistoryItem() const;

 protected:
  GURL url_;
  std::wstring title_;
  std::string history_state_;
  mutable RefPtr<WebCore::HistoryItem> history_item_;
  scoped_refptr<WebRequest::ExtraData> extra_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebHistoryItemImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H_
