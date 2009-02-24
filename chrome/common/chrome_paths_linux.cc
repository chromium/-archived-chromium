// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <glib.h>
#include "base/file_path.h"
#include "base/logging.h"

namespace {

// |env_name| is the name of an environment variable that we want to use to get
// a directory path. |fallback_dir| is the directory relative to $HOME that we
// use if |env_name| cannot be found or is empty. |fallback_dir| may be NULL.
// TODO(thestig): Don't use g_getenv() here because most of the time XDG
// environment variables won't actually be loaded.
FilePath GetStandardDirectory(const char* env_name, const char* fallback_dir) {
  FilePath rv;
  const char* env_value = g_getenv(env_name);
  if (env_value && env_value[0]) {
    rv = FilePath(env_value);
  } else {
    const char* home_dir = g_getenv("HOME");
    if (!home_dir)
      home_dir = g_get_home_dir();
    rv = FilePath(home_dir);
    if (fallback_dir)
      rv = rv.Append(fallback_dir);
  }

  return rv;
}

}  // namespace

namespace chrome {

// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go.  The net effect for most
// systems is we use ~/.config/chromium/ for Chromium and
// ~/.config/google-chrome/ for official builds.
// (This also helps us sidestep issues with other apps grabbing ~/.chromium .)
bool GetDefaultUserDataDirectory(FilePath* result) {
  FilePath config_dir = GetStandardDirectory("XDG_CONFIG_HOME", ".config");
#if defined(GOOGLE_CHROME_BUILD)
  *result = config_dir.Append("google-chrome");
#else
  *result = config_dir.Append("chromium");
#endif
  return true;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  *result = GetStandardDirectory("XDG_DOCUMENTS_DIR", "Documents");
  return true;
}

bool GetUserDesktop(FilePath* result) {
  *result = GetStandardDirectory("XDG_DESKTOP_DIR", "Desktop");
  return true;
}

}  // namespace chrome
