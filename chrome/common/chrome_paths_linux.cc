// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include "base/logging.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  // TODO(port): Decide what to do on linux.
  NOTIMPLEMENTED();
  return false;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  // TODO(port): Get the path (possibly using xdg-user-dirs)
  // or decide we don't need it on other platforms.
  NOTIMPLEMENTED();
  return false;
}

bool GetUserDesktop(FilePath* result) {
  // TODO(port): Get the path (possibly using xdg-user-dirs)
  // or decide we don't need it on other platforms.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace chrome
