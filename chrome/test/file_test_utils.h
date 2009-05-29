// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FILE_TEST_UTILS_H_
#define CHROME_TEST_FILE_TEST_UTILS_H_

#include "base/file_path.h"
#include "base/file_util.h"

// Auto deletes file/folder when it goes out-of-scope. This is useful for tests
// to cleanup files/folder automatically.
class FileAutoDeleter {
 public:
  explicit FileAutoDeleter(const FilePath& path);
  ~FileAutoDeleter();

  const FilePath& path() { return path_; }
 private:
  FilePath path_;
  DISALLOW_EVIL_CONSTRUCTORS(FileAutoDeleter);
};

#endif  // CHROME_TEST_FILE_TEST_UTILS_H_
