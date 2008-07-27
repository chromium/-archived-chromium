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

#ifndef WEBKIT_GLUE_WEBHISTORYITEM_H__
#define WEBKIT_GLUE_WEBHISTORYITEM_H__

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
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

#endif  // #ifndef WEBKIT_GLUE_WEBHISTORYITEM_H__
