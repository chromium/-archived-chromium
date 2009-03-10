// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LZMA_UTIL_H__
#define CHROME_INSTALLER_UTIL_LZMA_UTIL_H__

#include <string>
#include <windows.h>

#include "base/basictypes.h"

namespace installer {

// This is a utility class that acts as a wrapper around LZMA SDK library
class LzmaUtil {
 public:
  LzmaUtil();
  ~LzmaUtil();

  DWORD OpenArchive(const std::wstring& archivePath);

  // Unpacks the archive to the given location
  DWORD UnPack(const std::wstring& location);

  void CloseArchive();

 private:
  HANDLE archive_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(LzmaUtil);
};

}  // namespace installer


#endif  // CHROME_INSTALLER_UTIL_LZMA_UTIL_H__
