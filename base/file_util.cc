// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/file_util.h"

#include <fstream>

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

void ReplaceExtension(std::wstring* file_name, const std::wstring& extension) {
  const std::wstring::size_type last_dot = file_name->rfind(L'.');
  std::wstring result = file_name->substr(0, last_dot);
  if (!extension.empty() && extension != L".") {
    if (extension.at(0) != L'.')
      result.append(L".");
    result.append(extension);
  }
  file_name->swap(result);
}

bool ContentsEqual(const std::wstring& filename1,
                   const std::wstring& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
#if defined(OS_WIN)
  std::ifstream file1(filename1.c_str(), std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.c_str(), std::ios::in | std::ios::binary);
#elif defined(OS_POSIX)
  std::ifstream file1(WideToUTF8(filename1).c_str(),
                      std::ios::in | std::ios::binary);
  std::ifstream file2(WideToUTF8(filename2).c_str(),
                      std::ios::in | std::ios::binary);
#endif
  
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
#if defined(OS_WIN)
  FILE* file;
  errno_t err = _wfopen_s(&file, path.c_str(), L"rbS");
  if (err != 0)
    return false;
#elif defined(OS_POSIX)
  FILE* file = fopen(WideToUTF8(path).c_str(), "r");
  if (!file)
    return false;
#endif

  char buf[1 << 16];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
    contents->append(buf, len);
  }
  fclose(file);

  return true;
}

}  // namespace
