// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
