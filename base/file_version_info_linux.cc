// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"

#include <string>

#include "base/logging.h"

// TODO(port): Replace stubs with real implementations. We need a non-NULL
// FileVersionInfo object that reads info about the current binary. This is
// relatively easy to do under Windows, as there's some win32 API functions
// that return that information and Microsoft encourages developers to fill out
// that standard information block.
//
// We can't return NULL because that is used as a catastrophic error code where
// the file doesn't exist or can't be opened.

FileVersionInfo::FileVersionInfo() {}

FileVersionInfo::~FileVersionInfo() {}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return new FileVersionInfo();
}

std::wstring FileVersionInfo::company_name() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::company_short_name() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::product_name() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::product_short_name() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::internal_name() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::product_version() {
  // When un-stubbing, implementation in file_version_info.cc should be ok.
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"0.1.2.3.4.5.6-lie";
}

std::wstring FileVersionInfo::private_build() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::special_build() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::comments() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::original_filename() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::file_description() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::file_version() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"0.1.2.3.4.5.6-lie";
}

std::wstring FileVersionInfo::legal_copyright() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::legal_trademarks() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"";
}

std::wstring FileVersionInfo::last_change() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return L"Last Thursday";
}

bool FileVersionInfo::is_official_build() {
  NOTIMPLEMENTED() << "The current FileVersionInfo is a lie.";
  return false;
}
