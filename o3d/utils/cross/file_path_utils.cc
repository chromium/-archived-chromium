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


// This file contains the definitions for some convenience functions
// used to make FilePaths more useful.

#include "utils/cross/file_path_utils.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/file_path.h"

namespace o3d {
std::wstring FilePathToWide(const FilePath& input) {
#if defined(OS_WIN)
  return input.value();
#else
  return UTF8ToWide(input.value());
#endif
}

FilePath WideToFilePath(const std::wstring& input) {
#if defined(OS_WIN)
  return FilePath(input);
#else
  return FilePath(WideToUTF8(input));
#endif
}

String FilePathToUTF8(const FilePath& input) {
#if defined(OS_WIN)
  return WideToUTF8(input.value());
#else
  return input.value();
#endif
}

FilePath UTF8ToFilePath(const String& input) {
#if defined(OS_WIN)
  return FilePath(UTF8ToWide(input));
#else
  return FilePath(input);
#endif
}

bool AbsolutePath(FilePath* absolute_path) {
#if defined(OS_WIN)
  return file_util::AbsolutePath(absolute_path);
#else
  // On the Posix implementation of file_util::AbsolutePath,
  // realpath() is used, which only works if the path actually exists.
  // So, we try using AbsolutePath, and if it doesn't work, we fake it
  // by just prepending the current working direcory if it's not
  // already absolute.
  if (absolute_path->IsAbsolute()) {
    return true;
  } else {
    FilePath new_absolute_path = FilePath(*absolute_path);
    if (file_util::AbsolutePath(&new_absolute_path)) {
      *absolute_path = new_absolute_path;
      return true;
    } else {
      // OK, so we failed to make an absolute path above, and we know
      // it's not an absolute path to begin with, so we just prepend
      // the current working directory.
      FilePath cwd_path;
      if (!file_util::GetCurrentDirectory(&cwd_path)) {
        return false;
      }
      *absolute_path = cwd_path.Append(*absolute_path);
      return true;
    }
  }
#endif
}

bool GetRelativePathIfPossible(const FilePath& base_dir,
                               const FilePath& candidate,
                               FilePath *result) {
  FilePath parent = FilePath(base_dir.StripTrailingSeparators());
  FilePath child = FilePath(candidate.StripTrailingSeparators());

  // If we can't convert the child to an absolute path for some
  // reason, then we just do nothing and return the candidate.
  if (!child.IsAbsolute() && !AbsolutePath(&child)) {
    *result = candidate;
    return false;
  }

  // If we can't convert the parent to an absolute path for some
  // reason, then we just do nothing and return the absolute path to
  // the child.
  if (!parent.IsAbsolute() && !AbsolutePath(&parent)) {
    *result = child;
    return false;
  }

  FilePath::StringType child_str = child.value();
  FilePath::StringType parent_str = parent.value();

  // If the child is too short, it can't be a child of parent, and if
  // it doesn't have a separator in the right place, then it also
  // can't be a child, so we return the absolute path to the child.
  // file_util::AbsolutePath() normalizes '/' to '\' on Windows, so we
  // only need to check kSeparators[0].
  if (child_str.length() <= parent_str.length() ||
      child_str[parent_str.length()] != FilePath::kSeparators[0]) {
    *result = child;
    return false;
  }

  if (
#if defined(OS_WIN)
      // file_util::AbsolutePath() does not flatten case on Windows,
      // so we must do a case-insensitive compare.
      StartsWith(child_str, parent_str, false)
#else
      StartsWithASCII(child_str, parent_str, true)
#endif
      ) {
    // Add one to skip over the dir separator.
    child_str = child_str.substr(parent_str.size() + 1);

    // Now we know we can return the relative path.
    *result = FilePath(child_str);
    return true;
  } else {
    *result = FilePath(child_str);
    return false;
  }
}

}  // end namespace o3d
