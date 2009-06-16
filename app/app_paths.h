// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef APP_APP_PATHS_H_
#define APP_APP_PATHS_H_

// This file declares path keys for the app module.  These can be used with
// the PathService to access various special directories and files.

namespace app {

enum {
  PATH_START = 2000,

  DIR_THEMES,               // Directory where theme dll files are stored.
  DIR_LOCALES,              // Directory where locale resources are stored.
  DIR_EXTERNAL_EXTENSIONS,  // Directory where installer places .crx files.

  // Valid only in development environment; TODO(darin): move these
  DIR_TEST_DATA,            // Directory where unit test data resides.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace app

#endif  // APP_APP_PATHS_H_
