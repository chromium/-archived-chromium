// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  return true;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  // TODO(port)
  NOTIMPLEMENTED();
  return false;
}

bool GetUserDownloadsDirectory(FilePath* result) {
  // TODO(port)
  NOTIMPLEMENTED();
  return false;
}

bool GetUserDesktop(FilePath* result) {
  // TODO(port)
  NOTIMPLEMENTED();
  return false;
}

}  // namespace chrome
