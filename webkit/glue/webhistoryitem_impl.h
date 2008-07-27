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

#ifndef WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H__
#define WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H__

#include "base/basictypes.h"
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
  DISALLOW_EVIL_CONSTRUCTORS(WebHistoryItemImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBHISTORYITEM_IMPL_H__
