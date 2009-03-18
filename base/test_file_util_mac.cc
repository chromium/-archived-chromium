// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test_file_util.h"

#include "base/logging.h"

namespace file_util {

bool EvictFileFromSystemCache(const FilePath& file) {
  // There is no way that we can think of to dump something from the UBC.  You
  // can add something so when you open it, it doesn't go in, but that won't
  // work here.
  return true;
}

}  // namespace file_util
