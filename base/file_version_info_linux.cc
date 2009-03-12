// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"

#include <string>

// TODO(port): This whole file is stubbed. We can't return NULL
// because that is used as a catastrophic error code where the file
// doesn't exist or can't be opened.
//
// See http://code.google.com/p/chromium/issues/detail?id=8132 for a discussion
// on what we should do with this module.

FileVersionInfo::FileVersionInfo() {}

FileVersionInfo::~FileVersionInfo() {}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  return new FileVersionInfo();
}

std::wstring FileVersionInfo::company_name() {
  return L"";
}

std::wstring FileVersionInfo::company_short_name() {
  return L"";
}

std::wstring FileVersionInfo::product_name() {
  return L"";
}

std::wstring FileVersionInfo::product_short_name() {
  return L"";
}

std::wstring FileVersionInfo::internal_name() {
  return L"";
}

std::wstring FileVersionInfo::product_version() {
  return L"0.1.2.3.4.5.6-lie";
}

std::wstring FileVersionInfo::private_build() {
  return L"";
}

std::wstring FileVersionInfo::special_build() {
  return L"";
}

std::wstring FileVersionInfo::comments() {
  return L"";
}

std::wstring FileVersionInfo::original_filename() {
  return L"";
}

std::wstring FileVersionInfo::file_description() {
  return L"";
}

std::wstring FileVersionInfo::file_version() {
  return L"0.1.2.3.4.5.6-lie";
}

std::wstring FileVersionInfo::legal_copyright() {
  return L"";
}

std::wstring FileVersionInfo::legal_trademarks() {
  return L"";
}

std::wstring FileVersionInfo::last_change() {
  return L"Last Thursday";
}

bool FileVersionInfo::is_official_build() {
  return false;
}
