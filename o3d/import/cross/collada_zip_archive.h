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


// A basic C++ wrapper for a collada zip file
// it looks for the first .dae file in the archive and is able to resolve
// partial pathnames (from the URI's in the collada file) to files in the
// archive

#ifndef O3D_IMPORT_CROSS_COLLADA_ZIP_ARCHIVE_H_
#define O3D_IMPORT_CROSS_COLLADA_ZIP_ARCHIVE_H_

#include "import/cross/zip_archive.h"

#include <string>
#include <vector>

namespace o3d {

class ColladaZipArchive : public ZipArchive {
 public:
  ColladaZipArchive(const std::string &zip_filename, int *result);

  // |filename| is taken to be relative to the directory containing the
  // first collada file found in the archive.  It may contain relative path
  // elements ("../").  These are the types of file references to images
  // contained in the collada file.
  //
  // Extracts a single file and returns a pointer to the file's content.
  // Returns NULL if |filename| doesn't match any in the archive
  // or an error occurs.  The caller must call free() on the returned pointer
  //
  virtual char  *GetColladaAssetData(const std::string &filename,
                                     size_t *size);

  const std::string& GetColladaPath() const { return dae_pathname_; }
  const std::string& GetColladaDirectory() const { return dae_directory_; }

 protected:
  std::string dae_pathname_;
  std::string dae_directory_;
};
}  // end namespace o3d

#endif  // O3D_IMPORT_CROSS_COLLADA_ZIP_ARCHIVE_H_
