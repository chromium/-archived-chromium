// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace base {

bool PathProvider(int key, std::wstring* result) {
  // NOTE: DIR_CURRENT is a special cased in PathService::Get

  std::wstring cur;
  switch (key) {
    case base::DIR_EXE:
      PathService::Get(base::FILE_EXE, &cur);
      file_util::TrimFilename(&cur);
      break;
    case base::DIR_MODULE:
      PathService::Get(base::FILE_MODULE, &cur);
      file_util::TrimFilename(&cur);
      break;
    case base::DIR_TEMP:
      if (!file_util::GetTempDir(&cur))
        return false;
      break;
    default:
      return false;
  }

  result->swap(cur);
  return true;
}

}  // namespace base

