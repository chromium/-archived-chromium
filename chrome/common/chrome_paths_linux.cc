// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <glib.h>
#include "base/file_path.h"
#include "base/logging.h"

namespace chrome {

// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go.  The net effect for most
// systems is we use ~/.config/chromium/ for Chromium and
// ~/.config/google-chrome/ for official builds.
// (This also helps us sidestep issues with other apps grabbing ~/.chromium .)
bool GetDefaultUserDataDirectory(FilePath* result) {
  FilePath config_dir;
  const char* config_home = g_getenv("XDG_CONFIG_HOME");
  if (config_home && config_home[0]) {
    config_dir = FilePath(config_home);
  } else {
    const char* home_dir = g_getenv("HOME");
    if (!home_dir)
      home_dir = g_get_home_dir();
    config_dir = FilePath(home_dir).Append(".config");
  }

#if defined(GOOGLE_CHROME_BUILD)
  *result = config_dir.Append("google-chrome");
#else
  *result = config_dir.Append("chromium");
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
