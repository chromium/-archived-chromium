// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"

// These includes are just for the *Hack functions, and should be removed
// when those functions are removed.
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
const FilePath::CharType FilePath::kSeparators[] = FILE_PATH_LITERAL("\\/");
#else  // FILE_PATH_USES_WIN_SEPARATORS
const FilePath::CharType FilePath::kSeparators[] = FILE_PATH_LITERAL("/");
#endif  // FILE_PATH_USES_WIN_SEPARATORS

const FilePath::CharType FilePath::kCurrentDirectory[] = FILE_PATH_LITERAL(".");
const FilePath::CharType FilePath::kParentDirectory[] = FILE_PATH_LITERAL("..");

// Returns true if |character| is in kSeparators.
static bool IsSeparator(FilePath::CharType character) {
  for (size_t i = 0; i < arraysize(FilePath::kSeparators) - 1; ++i) {
    if (character == FilePath::kSeparators[i]) {
      return true;
    }
  }

  return false;
}

// libgen's dirname and basename aren't guaranteed to be thread-safe and aren't
// guaranteed to not modify their input strings, and in fact are implmeneted
// differently in this regard on different platforms.  Don't use them, but
// adhere to their behavior.
FilePath FilePath::DirName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparators();

  // The drive letter, if any, always needs to remain in the output.  If there
  // is no drive letter, as will always be the case on platforms which do not
  // support drive letters, letter will be npos, or -1, so the comparisons and
  // resizes below using letter will still be valid.
  StringType::size_type letter = new_path.FindDriveLetter();

  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  arraysize(kSeparators) - 1);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    new_path.path_.resize(letter + 1);
  } else if (last_separator == letter + 1) {
    // path_ is in the root directory.
    new_path.path_.resize(letter + 2);
  } else if (last_separator == letter + 2 &&
             IsSeparator(new_path.path_[letter + 1])) {
    // path_ is in "//" (possibly with a drive letter); leave the double
    // separator intact indicating alternate root.
    new_path.path_.resize(letter + 3);
  } else if (last_separator != 0) {
    // path_ is somewhere else, trim the basename.
    new_path.path_.resize(last_separator);
  }

  new_path.StripTrailingSeparators();
  if (!new_path.path_.length())
    new_path.path_ = kCurrentDirectory;

  return new_path;
}

FilePath FilePath::BaseName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparators();

  // The drive letter, if any, is always stripped.
  StringType::size_type letter = new_path.FindDriveLetter();
  if (letter != StringType::npos) {
    new_path.path_.erase(0, letter + 1);
  }

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  arraysize(kSeparators) - 1);
  if (last_separator != StringType::npos &&
      last_separator < new_path.path_.length() - 1) {
    new_path.path_.erase(0, last_separator + 1);
  }

  return new_path;
}

FilePath FilePath::Append(const FilePath::StringType& component) const {
  if (path_.compare(kCurrentDirectory) == 0) {
    // Append normally doesn't do any normalization, but as a special case,
    // when appending to kCurrentDirectory, just return a new path for the
    // component argument.  Appending component to kCurrentDirectory would
    // serve no purpose other than needlessly lengthening the path, and
    // it's likely in practice to wind up with FilePath objects containing
    // only kCurrentDirectory when calling DirName on a single relative path
    // component.
    return FilePath(component);
  }

  FilePath new_path(path_);
  new_path.StripTrailingSeparators();

  // Don't append a separator if the path is empty (indicating the current
  // directory) or if the path component is empty (indicating nothing to
  // append).
  if (component.length() > 0 && new_path.path_.length() > 0) {

    // Don't append a separator if the path still ends with a trailing
    // separator after stripping (indicating the root directory).
    if (!IsSeparator(new_path.path_[new_path.path_.length() - 1])) {

      // Don't append a separator if the path is just a drive letter.
      if (new_path.FindDriveLetter() + 1 != new_path.path_.length()) {
        new_path.path_.append(1, kSeparators[0]);
      }
    }
  }

  new_path.path_.append(component);
  return new_path;
}

FilePath::StringType::size_type FilePath::FindDriveLetter() const {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  // This is dependent on an ASCII-based character set, but that's a
  // reasonable assumption.  iswalpha can be too inclusive here.
  if (path_.length() >= 2 && path_[1] == L':' &&
      ((path_[0] >= L'A' && path_[0] <= L'Z') ||
       (path_[0] >= L'a' && path_[0] <= L'z'))) {
    return 1;
  }
  return StringType::npos;
#else  // FILE_PATH_USES_DRIVE_LETTERS
  return StringType::npos;
#endif  // FILE_PATH_USES_DRIVE_LETTERS
}

bool FilePath::IsAbsolute() const {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  StringType::size_type letter = FindDriveLetter();
  if (letter != StringType::npos) {
    // Look for a separator right after the drive specification.
    return path_.length() > letter + 1 && IsSeparator(path_[letter + 1]);
  }
  // Look for a pair of leading separators.
  return path_.length() > 1 && IsSeparator(path_[0]) && IsSeparator(path_[1]);
#else  // FILE_PATH_USES_DRIVE_LETTERS
  // Look for a separator in the first position.
  return path_.length() > 0 && IsSeparator(path_[0]);
#endif  // FILE_PATH_USES_DRIVE_LETTERS
}

#if defined(OS_POSIX)
// See file_path.h for a discussion of the encoding of paths on POSIX
// platforms.  These *Hack() functions are not quite correct, but they're
// only temporary while we fix the remainder of the code.
// Remember to remove the #includes at the top when you remove these.

// static
FilePath FilePath::FromWStringHack(const std::wstring& wstring) {
  return FilePath(base::SysWideToNativeMB(wstring));
}
std::wstring FilePath::ToWStringHack() const {
  return base::SysNativeMBToWide(path_);
}
#elif defined(OS_WIN)
// static
FilePath FilePath::FromWStringHack(const std::wstring& wstring) {
  return FilePath(wstring);
}
std::wstring FilePath::ToWStringHack() const {
  return path_;
}
#endif

void FilePath::StripTrailingSeparators() {
  // If there is no drive letter, start will be 1, which will prevent stripping
  // the leading separator if there is only one separator.  If there is a drive
  // letter, start will be set appropriately to prevent stripping the first
  // separator following the drive letter, if a separator immediately follows
  // the drive letter.
  StringType::size_type start = FindDriveLetter() + 2;

  StringType::size_type last_stripped = StringType::npos;
  for (StringType::size_type pos = path_.length();
       pos > start && IsSeparator(path_[pos - 1]);
       --pos) {
    // If the string only has two separators and they're at the beginning,
    // don't strip them, unless the string began with more than two separators.
    if (pos != start + 1 || last_stripped == start + 2 ||
        !IsSeparator(path_[start - 1])) {
      path_.resize(pos - 1);
      last_stripped = pos;
    }
  }
}
