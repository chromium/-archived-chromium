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


// This file contains the declaration for some convenience functions
// used to make FilePaths more useful.

#ifndef O3D_UTILS_CROSS_FILE_PATH_UTILS_H_
#define O3D_UTILS_CROSS_FILE_PATH_UTILS_H_

#include <string>
#include "core/cross/types.h"

class FilePath;

namespace o3d {
// TODO: Go through the process to add these to FilePath
// itself in the Chromium depot.

std::wstring FilePathToWide(const FilePath& input);
FilePath WideToFilePath(const std::wstring& input);
String FilePathToUTF8(const FilePath& input);
FilePath UTF8ToFilePath(const String& input);

// On Windows, this is just the same as file_util::AbsolutePath.
// On the Posix implementation of file_util::AbsolutePath,
// realpath() is used, which only works if the path actually exists.
// So, we try using AbsolutePath, and if it doesn't work, we fake it
// by just prepending the cwd if it's not already an absolute path.
bool AbsolutePath(FilePath* abs_path);

// If the candidate is a child (a file or directory in a subdir of the
// base directory or the base directory itself), then we figure out
// the relative path to it.  If not, then we just return the absolute
// path to the candidate.  Does not return any paths that would
// require use of ".." to form a relative path.  Returns true if the
// path returned in "result" is a relative path.
bool GetRelativePathIfPossible(const FilePath& base_dir,
                               const FilePath& candidate,
                               FilePath *result);

}  // namespace o3d

#endif  // O3D_UTILS_CROSS_FILE_PATH_UTILS_H_
