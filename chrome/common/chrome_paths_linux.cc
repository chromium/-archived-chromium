// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <glib.h>
#include "base/file_path.h"
#include "base/logging.h"

namespace chrome {

// Use ~/.chromium/ for Chromium and ~/.google/chrome for official builds.  We
// use ~/.google/chrome because ~/.google/ is already used by other Google
// Linux apps.
bool GetDefaultUserDataDirectory(FilePath* result) {
  // The glib documentation says that g_get_home_dir doesn't honor HOME so we
  // should try that first.
  const char* home_dir = g_getenv("HOME");
  if (!home_dir)
    home_dir = g_get_home_dir();
  *result = FilePath(home_dir);
#if defined(GOOGLE_CHROME_BUILD)
  *result = result->Append(".google");
  *result = result->Append("chrome");
#else
  *result = result->Append(".chromium");
#endif
  return true;
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
