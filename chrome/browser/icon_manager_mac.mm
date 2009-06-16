// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include "base/file_path.h"
#include "base/sys_string_conversions.h"

IconGroupID IconManager::GetGroupIDFromFilepath(const FilePath& filepath) {
  return filepath.Extension();
}
