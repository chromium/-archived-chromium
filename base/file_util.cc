// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <stdio.h>

#include <fstream>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "unicode/uniset.h"

namespace file_util {

const wchar_t kExtensionSeparator = L'.';

void PathComponents(const std::wstring& path,
                    std::vector<std::wstring>* components) {
  DCHECK(components != NULL);
  if (components == NULL)
    return;
  std::wstring::size_type start = 0;
  std::wstring::size_type end = path.find(kPathSeparator, start);

  // Special case the "/" or "\" directory.  On Windows with a drive letter,
  // this code path won't hit, but the right thing should still happen.  
  // "E:\foo" will turn into "E:","foo".
  if (end == start) {
    components->push_back(std::wstring(path, 0, 1));
    start = end + 1;
    end = path.find(kPathSeparator, start);
  }
  while (end != std::wstring::npos) {
    std::wstring component = std::wstring(path, start, end - start);
    components->push_back(component);
    start = end + 1;
    end = path.find(kPathSeparator, start);
  }
  std::wstring component = std::wstring(path, start);
  components->push_back(component);
}
  
bool EndsWithSeparator(std::wstring* path) {
  return EndsWithSeparator(*path);
}
  
bool EndsWithSeparator(const std::wstring& path) {
  bool is_sep = (path.length() > 0 && 
      (path)[path.length() - 1] == kPathSeparator);
  return is_sep;
}

void TrimTrailingSeparator(std::wstring* dir) {
  while (dir->length() > 1 && EndsWithSeparator(dir))
    dir->resize(dir->length() - 1);
}

void UpOneDirectory(std::wstring* dir) {
  TrimTrailingSeparator(dir);

  std::wstring::size_type last_sep = dir->find_last_of(kPathSeparator);
  if (last_sep != std::wstring::npos)
    dir->resize(last_sep);
}

void UpOneDirectoryOrEmpty(std::wstring* dir) {
  TrimTrailingSeparator(dir);

  std::wstring::size_type last_sep = dir->find_last_of(kPathSeparator);
  if (last_sep != std::wstring::npos)
    dir->resize(last_sep);
  else
    dir->clear();
}

void TrimFilename(std::wstring* path) {
  if (EndsWithSeparator(path)) {
    TrimTrailingSeparator(path);
  } else {
    std::wstring::size_type last_sep = path->find_last_of(kPathSeparator);
    if (last_sep != std::wstring::npos)
      path->resize(last_sep);
  }
}

std::wstring GetFilenameFromPath(const std::wstring& path) {
  // TODO(erikkay): fix this - it's not using kPathSeparator, but win unit test
  // are exercising '/' as a path separator as well.
  std::wstring::size_type pos = path.find_last_of(L"\\/");
  return std::wstring(path, pos == std::wstring::npos ? 0 : pos+1);
}

std::wstring GetFileExtensionFromPath(const std::wstring& path) {
  std::wstring file_name = GetFilenameFromPath(path);
  std::wstring::size_type last_dot = file_name.rfind(L'.');
  return std::wstring(last_dot == std::wstring::npos? L"" : file_name, last_dot+1);
}

void AppendToPath(std::wstring* path, const std::wstring& new_ending) {
  if (!path) {
    NOTREACHED();
    return;  // Don't crash in this function in release builds.
  }

  if (!EndsWithSeparator(path))
    path->push_back(kPathSeparator);
  path->append(new_ending);
}

void InsertBeforeExtension(std::wstring* path, const std::wstring& suffix) {
  DCHECK(path);

  const std::wstring::size_type last_dot = path->rfind(kExtensionSeparator);
  const std::wstring::size_type last_sep = path->rfind(kPathSeparator);

  if (last_dot == std::wstring::npos ||
      (last_sep != std::wstring::npos && last_dot < last_sep)) {
    // The path looks something like "C:\pics.old\jojo" or "C:\pics\jojo".
    // We should just append the suffix to the entire path.
    path->append(suffix);
    return;
  }

  path->insert(last_dot, suffix);
}

void ReplaceIllegalCharacters(std::wstring* file_name, int replace_char) {
  DCHECK(file_name);

  // Control characters, formatting characters, non-characters, and
  // some printable ASCII characters regarded as dangerous ('"*/:<>?\\').
  // See  http://blogs.msdn.com/michkap/archive/2006/11/03/941420.aspx
  // and http://msdn2.microsoft.com/en-us/library/Aa365247.aspx
  // TODO(jungshik): Revisit the set. ZWJ and ZWNJ are excluded because they
  // are legitimate in Arabic and some S/SE Asian scripts. However, when used
  // elsewhere, they can be confusing/problematic.
  // Also, consider wrapping the set with our Singleton class to create and
  // freeze it only once. Note that there's a trade-off between memory and
  // speed.

  UErrorCode status = U_ZERO_ERROR;
#if defined(WCHAR_T_IS_UTF16)
  UnicodeSet illegal_characters(UnicodeString(
      L"[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\u200c\u200d]]"), status);
#else
  UnicodeSet illegal_characters(UNICODE_STRING_SIMPLE(
      "[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\\u200c\\u200d]]").unescape(), status);
#endif
  DCHECK(U_SUCCESS(status));
  // Add non-characters. If this becomes a performance bottleneck by
  // any chance, check |ucs4 & 0xFFFEu == 0xFFFEu|, instead.
  illegal_characters.add(0xFDD0, 0xFDEF);
  for (int i = 0; i <= 0x10; ++i) {
    int plane_base = 0x10000 * i;
    illegal_characters.add(plane_base + 0xFFFE, plane_base + 0xFFFF);
  }
  illegal_characters.freeze();
  DCHECK(!illegal_characters.contains(replace_char) && replace_char < 0x10000);

  // Remove leading and trailing whitespace.
  TrimWhitespace(*file_name, TRIM_ALL, file_name);

  std::wstring::size_type i = 0;
  std::wstring::size_type length = file_name->size();
  const wchar_t* wstr = file_name->data();
#if defined(WCHAR_T_IS_UTF16)
  // Using |span| method of UnicodeSet might speed things up a bit, but
  // it's not likely to matter here.
  std::wstring temp;
  temp.reserve(length);
  while (i < length) {
    UChar32 ucs4;
    std::wstring::size_type prev = i;
    U16_NEXT(wstr, i, length, ucs4);
    if (illegal_characters.contains(ucs4)) {
      temp.push_back(replace_char);
    } else if (ucs4 < 0x10000) {
      temp.push_back(ucs4);
    } else {
      temp.push_back(wstr[prev]);
      temp.push_back(wstr[prev + 1]);
    }
  }
  file_name->swap(temp);
#elif defined(WCHAR_T_IS_UTF32)
  while (i < length) {
    if (illegal_characters.contains(wstr[i])) {
      (*file_name)[i] = replace_char;
    }
    ++i;
  }
#else
#error wchar_t* should be either UTF-16 or UTF-32
#endif
}

// Appends the extension to file adding a '.' if extension doesn't contain one.
// This does nothing if extension is empty or '.'. This is used internally by
// ReplaceExtension.
static void AppendExtension(const std::wstring& extension,
                            std::wstring* file) {
  if (!extension.empty() && extension != L".") {
    if (extension[0] != L'.')
      file->append(L".");
    file->append(extension);
  }
}

void ReplaceExtension(std::wstring* file_name, const std::wstring& extension) {
  const std::wstring::size_type last_dot = file_name->rfind(L'.');
  if (last_dot == std::wstring::npos) {
    // No extension, just append the supplied extension.
    AppendExtension(extension, file_name);
    return;
  }
  const std::wstring::size_type last_separator =
      file_name->rfind(kPathSeparator);
  if (last_separator != std::wstring::npos && last_dot < last_separator) {
    // File name doesn't have extension, but one of the directories does; don't
    // replace it, just append the supplied extension. For example
    // 'c:\tmp.bar\foo'.
    AppendExtension(extension, file_name);
    return;
  }
  std::wstring result = file_name->substr(0, last_dot);
  AppendExtension(extension, &result);
  file_name->swap(result);
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
  std::ifstream file1(filename1.value().c_str(),
                      std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value().c_str(),
                      std::ios::in | std::ios::binary);
  
  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  const int BUFFER_SIZE = 2056;
  char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
  do {
    file1.read(buffer1, BUFFER_SIZE);
    file2.read(buffer2, BUFFER_SIZE);

    if ((file1.eof() && !file2.eof()) ||
        (!file1.eof() && file2.eof()) ||
        (file1.gcount() != file2.gcount()) ||
        (memcmp(buffer1, buffer2, file1.gcount()))) {
      file1.close();
      file2.close();
      return false;
    }
  } while (!file1.eof() && !file2.eof());

  file1.close();
  file2.close();
  return true;
}

bool ReadFileToString(const std::wstring& path, std::string* contents) {
  FILE* file = OpenFile(path, "rb");
  if (!file) {
    return false;
  }

  char buf[1 << 16];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
    contents->append(buf, len);
  }
  CloseFile(file);

  return true;
}

bool GetFileSize(const std::wstring& file_path, int64* file_size) {
  FileInfo info;
  if (!GetFileInfo(file_path, &info))
    return false;
  *file_size = info.size;
  return true;
}

bool CloseFile(FILE* file) {
  if (file == NULL)
    return true;
  return fclose(file) == 0;
}

// Deprecated functions ----------------------------------------------------

bool AbsolutePath(std::wstring* path_str) {
  FilePath path;
  if (!AbsolutePath(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}
bool Delete(const std::wstring& path, bool recursive) {
  return Delete(FilePath::FromWStringHack(path), recursive);
}
bool Move(const std::wstring& from_path, const std::wstring& to_path) {
  return Move(FilePath::FromWStringHack(from_path),
              FilePath::FromWStringHack(to_path));
}
bool CopyFile(const std::wstring& from_path, const std::wstring& to_path) {
  return CopyFile(FilePath::FromWStringHack(from_path),
                  FilePath::FromWStringHack(to_path));
}
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive) {
  return CopyDirectory(FilePath::FromWStringHack(from_path),
                       FilePath::FromWStringHack(to_path),
                       recursive);
}
bool PathExists(const std::wstring& path) {
  return PathExists(FilePath::FromWStringHack(path));
}
bool DirectoryExists(const std::wstring& path) {
  return DirectoryExists(FilePath::FromWStringHack(path));
}
bool ContentsEqual(const std::wstring& filename1,
                   const std::wstring& filename2) {
  return ContentsEqual(FilePath::FromWStringHack(filename1),
                       FilePath::FromWStringHack(filename2));
}
bool CreateDirectory(const std::wstring& full_path) {
  return CreateDirectory(FilePath::FromWStringHack(full_path));
}
bool GetCurrentDirectory(std::wstring* path_str) {
  FilePath path;
  if (!GetCurrentDirectory(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}
bool GetTempDir(std::wstring* path_str) {
  FilePath path;
  if (!GetTempDir(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}

}  // namespace

