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


// This file declares a simple manager class that manages a temporary
// file.

#ifndef O3D_UTILS_CROSS_TEMPORARY_FILE_H_
#define O3D_UTILS_CROSS_TEMPORARY_FILE_H_

#include "base/file_path.h"

namespace o3d {

// TemporaryFile is a simple manager class that manages a temporary
// file.  It accepts the path to the temporary file when it is
// created, and deletes the temporary file when it goes out of scope.
// It is up to the creator to verify that the program has the rights
// to delete the file given to it.
class TemporaryFile {
 public:
  // Creates an empty TemporaryFile object.  Must call "reset" to have
  // it manage a file.
  TemporaryFile();

  // Manages the given file path as a temporary file.
  explicit TemporaryFile(const FilePath& file_to_manage);
  ~TemporaryFile();

  // Returns the currently managed path.
  const FilePath& path() const {
    return file_path_;
  }

  // Releases the managed path so that it will NOT be deleted when
  // this object goes out of scope.  The path that was being managed
  // is returned.
  FilePath Release();

  // Resets the path being managed to the supplied path.  If this
  // object was managing a file before, then that file will be
  // immediately deleted.
  void Reset(const FilePath& file_path);

  // Creates an empty temporary file in the system's default temporary
  // directory and resets the given TemporaryFile object to manage
  // that file.  Returns true if it was successful in creating the
  // file.
  static bool Create(TemporaryFile* temp_file);

 private:
  FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(TemporaryFile);
};

}  // end namespace o3d
#endif  // O3D_UTILS_CROSS_TEMPORARY_FILE_H_
