// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include "base/logging.h"
#include "base/path_service.h"

namespace media {

// Attempts to initialize the media library (loading DLLs, DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& module_dir) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
