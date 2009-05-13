// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBHISTORYITEM_H_
#define WEBKIT_GLUE_WEBHISTORYITEM_H_

#include "webkit/glue/weburlrequest.h"  // for WebRequest::ExtraData

class GURL;

class WebHistoryItem : public base::RefCounted<WebHistoryItem> {
 public:
  // Create a new history item.
  static WebHistoryItem* Create(const GURL& url,
                                const std::wstring& title,
                                const std::string& history_state,
                                WebRequest::ExtraData* extra_data);

  WebHistoryItem() { }
  virtual ~WebHistoryItem() { }

  // Returns the URL.
  virtual const GURL& GetURL() const = 0;

  // Returns the title.
  virtual const std::wstring& GetTitle() const = 0;

  // Returns the string representation of the history state for this entry.
  virtual const std::string& GetHistoryState() const = 0;

  // Returns any ExtraData associated with this history entry.
  virtual WebRequest::ExtraData* GetExtraData() const = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebHistoryItem);
};

#endif  // #ifndef WEBKIT_GLUE_WEBHISTORYITEM_H_
