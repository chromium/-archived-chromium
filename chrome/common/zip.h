// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ZIP_H_
#define CHROME_COMMON_ZIP_H_

#include <vector>

#include "base/file_path.h"

// Zip the contents of src_dir into dest_file. src_path must be a directory.
// An entry will *not* be created in the zip for the root folder -- children
// of src_dir will be at the root level of the created zip.
bool Zip(const FilePath& src_dir, const FilePath& dest_file);

// Unzip the contents of zip_file into dest_dir.
bool Unzip(const FilePath& zip_file, const FilePath& dest_dir);

#endif  // CHROME_COMMON_ZIP_H_
