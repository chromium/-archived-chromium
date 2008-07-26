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

#ifndef CHROME_BROWSER_PAGE_STATE_H__
#define CHROME_BROWSER_PAGE_STATE_H__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"

class GURL;

/////////////////////////////////////////////////////////////////////////////
//
// PageState
//
// PageState represents a collection of key value pairs that can be
// represented as an url or a byte array. It is used by synthetic pages such
// as the destination tab to store and parse navigation states
//
/////////////////////////////////////////////////////////////////////////////
class PageState {
 public:
  PageState() : state_(new DictionaryValue) {}
  ~PageState() {}

  // Init with the provided url
  void InitWithURL(const GURL& url);

  // Init with the provided bytes
  void InitWithBytes(const std::string& bytes);

  // Return a string representing this state
  void GetByteRepresentation(std::string* out) const;

  // Conveniences to set and retreive an int
  void SetIntProperty(const std::wstring& key, int value);
  bool GetIntProperty(const std::wstring& key, int* value) const;

  // Conveniences to set and retreive an int64.
  void SetInt64Property(const std::wstring& key, int64 value);
  bool GetInt64Property(const std::wstring& key, int64* value) const;

  // Set / Get string properties
  void SetProperty(const std::wstring& key, const std::wstring& value);
  bool GetProperty(const std::wstring& key, std::wstring* value) const;

  // Creates a copy of this page state. It is up to the caller to delete the
  // returned value.
  PageState* Copy() const;

 private:

  // our actual state collection
  scoped_ptr<DictionaryValue> state_;

  DISALLOW_EVIL_CONSTRUCTORS(PageState);
};


#endif  // CHROME_BROWSER_PAGE_STATE_H__
