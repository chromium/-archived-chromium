// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/file_test_utils.h"

FileAutoDeleter::FileAutoDeleter(const FilePath& path)
    : path_(path) {
}

FileAutoDeleter::~FileAutoDeleter() {
  file_util::Delete(path_, true);
}
