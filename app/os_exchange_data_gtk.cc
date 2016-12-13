// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/os_exchange_data.h"

#include "base/logging.h"

OSExchangeData::OSExchangeData() {
}

OSExchangeData::~OSExchangeData() {
}

void OSExchangeData::SetString(const std::wstring& data) {
  NOTIMPLEMENTED();
}

void OSExchangeData::SetURL(const GURL& url, const std::wstring& title) {
  NOTIMPLEMENTED();
}

void OSExchangeData::SetFilename(const std::wstring& full_path) {
  NOTIMPLEMENTED();
}

void OSExchangeData::SetFileContents(const std::wstring& filename,
                                     const std::string& file_contents) {
  NOTIMPLEMENTED();
}

void OSExchangeData::SetHtml(const std::wstring& html, const GURL& base_url) {
  NOTIMPLEMENTED();
}

bool OSExchangeData::GetString(std::wstring* data) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::GetURLAndTitle(GURL* url, std::wstring* title) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::GetFilename(std::wstring* full_path) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::GetFileContents(std::wstring* filename,
                                     std::string* file_contents) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::GetHtml(std::wstring* html, GURL* base_url) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::HasString() const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::HasURL() const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeData::HasFile() const {
  NOTIMPLEMENTED();
  return false;
}
