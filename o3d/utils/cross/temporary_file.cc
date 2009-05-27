/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the definition of class TemporaryFile.

#include "utils/cross/temporary_file.h"

#include "base/file_path.h"
#include "base/file_util.h"

namespace o3d {

namespace {
void DeletePath(const FilePath& path) {
  if (!path.empty()) {
    file_util::Delete(path, false);
  }
}
}  // end anonymous namespace

TemporaryFile::TemporaryFile() {
  // file_path_ is initialized to be empty.
}

TemporaryFile::TemporaryFile(const FilePath& file_to_manage)
    : file_path_(file_to_manage) {
}

bool TemporaryFile::Create(TemporaryFile* temporary_file) {
  FilePath temporary_path;
  if (file_util::CreateTemporaryFileName(&temporary_path)) {
    temporary_file->Reset(temporary_path);
  } else {
    return false;
  }
  return true;
}

TemporaryFile::~TemporaryFile() {
  DeletePath(file_path_);
}

FilePath TemporaryFile::Release() {
  FilePath old_path = file_path_;
  file_path_ = FilePath();
  return old_path;
}

void TemporaryFile::Reset(const FilePath& file_path) {
  DeletePath(file_path_);
  file_path_ = file_path;
}

}  // end namespace o3d
