// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"

#include <string>

#include "base/logging.h"

// TODO(port): Replace stubs with real implementations.

FileVersionInfo::~FileVersionInfo() {}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  NOTIMPLEMENTED();
  return NULL;
}

std::wstring FileVersionInfo::product_version() {
  // When un-stubbing, implementation in file_version_info.cc should be ok.
  NOTIMPLEMENTED();
  return L"";
}

std::wstring FileVersionInfo::file_version() {
  NOTIMPLEMENTED();
  return L"";
}

std::wstring FileVersionInfo::last_change() {
  NOTIMPLEMENTED();
  return L"";
}

bool FileVersionInfo::is_official_build() {
  NOTIMPLEMENTED();
  return false;
}
