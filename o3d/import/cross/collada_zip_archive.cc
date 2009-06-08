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
// partial pathnames (from the image URI's in the collada file) to files in the
// archive

#include "base/string_util.h"
#include "import/cross/collada_zip_archive.h"

using std::vector;
using std::string;

namespace o3d {

ColladaZipArchive::ColladaZipArchive(const std::string &zip_filename,
                                     int *result)
  : ZipArchive(zip_filename, result) {
  if (result && (*result == UNZ_OK)) {
    // look through the archive and locate the first file with a .dae extension
    vector<ZipFileInfo> infolist;
    GetInformationList(&infolist);

    bool dae_found = false;
    for (vector<ZipFileInfo>::size_type i = 0; i < infolist.size(); ++i) {
      const char *name = infolist[i].name.c_str();
      int length = strlen(name);

      if (length > 4) {
        const char *suffix = name + length - 4;
        if (!base::strcasecmp(suffix, ".dae")) {
          dae_pathname_ = name;
          dae_directory_ = dae_pathname_;
          RemoveLastPathComponent(&dae_directory_);
          dae_found = true;
          break;
        }
      }
    }

    if (!dae_found && result) *result = -1;
  }
}

// Convert paths relative to the collada file to archive paths
char  *ColladaZipArchive::GetColladaAssetData(const string &filename,
                                              size_t *size) {
  return GetRelativeFileData(filename, dae_directory_, size);
}
}  // end namespace o3d
