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

#include <sys/stat.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>

#include <fstream>

#include "base/logging.h"
#include "base/string_util.h"

namespace file_util {

std::wstring GetDirectoryFromPath(const std::wstring& path) {
  char full_path[PATH_MAX];
#if defined(OS_MACOSX)
  strlcpy(full_path, WideToUTF8(path).c_str(), sizeof(full_path));
#elif defined(OS_LINUX)
  std::string utf8_path = WideToUTF8(path);
  const char* cstr = utf8_path.c_str();
  strncpy(full_path, cstr, PATH_MAX);
  cstr[PATH_MAX - 1] = '\0';
#endif
  return UTF8ToWide(dirname(full_path));
}

bool Delete(const std::wstring& path, bool recursive) {
  std::string utf8_path = WideToUTF8(path);
  struct stat64 file_info;
  if (stat64(utf8_path.c_str(), &file_info) != 0);
    return false;
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(utf8_path.c_str()) == 0);
  if (!recursive)
    return (rmdir(utf8_path.c_str()) == 0);

  // TODO(erikkay): delete directories
  DCHECK(recursive);
  return false;
}

bool Move(const std::wstring& from_path, const std::wstring& to_path) {
  return (rename(WideToUTF8(from_path).c_str(),
                 WideToUTF8(to_path).c_str()) == 0);
}

bool CopyTree(const std::wstring& from_path, const std::wstring& to_path) {
  // TODO(erikkay): implement
  return false;
}

bool PathExists(const std::wstring& path) {
  struct stat64 file_info;
  return (stat64(WideToUTF8(path).c_str(), &file_info) == 0);
}

// TODO(erikkay): implement
#if 0
bool GetFileCreationLocalTimeFromHandle(int fd,
                                        LPSYSTEMTIME creation_time) {
  if (!file_handle)
    return false;
  
  FILETIME utc_filetime;
  if (!GetFileTime(file_handle, &utc_filetime, NULL, NULL))
    return false;
  
  FILETIME local_filetime;
  if (!FileTimeToLocalFileTime(&utc_filetime, &local_filetime))
    return false;
  
  return !!FileTimeToSystemTime(&local_filetime, creation_time);
}

bool GetFileCreationLocalTime(const std::string& filename,
                              LPSYSTEMTIME creation_time) {
  ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ, 
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}
#endif

bool ResolveShortcut(std::wstring* path) {
  char full_path[PATH_MAX];
  if (!realpath(WideToUTF8(*path).c_str(), full_path))
    return false;
  *path = UTF8ToWide(full_path);
  return true;
}

bool CreateShortcutLink(const char *source, const char *destination,
                        const char *working_dir, const char *arguments, 
                        const char *description, const char *icon,
                        int icon_index) {
  // TODO(erikkay): implement
  return false;
}

bool CreateTemporaryFileName(std::wstring* temp_file) {
  std::wstring tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  tmpdir.append(L"/com.google.chrome.XXXXXX");
  // this should be OK since mktemp just replaces characters in place
  char* buffer = const_cast<char*>(WideToUTF8(tmpdir).c_str());
  *temp_file = UTF8ToWide(mktemp(buffer));
  return true;
}

bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path) {
  std::wstring tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  tmpdir.append(L"/com.google.chrome.XXXXXX");
  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(WideToUTF8(tmpdir).c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp)
    return false;
  *new_temp_path = UTF8ToWide(dtemp);
  return true;
}

bool CreateDirectory(const std::wstring& full_path) {
  return (mkdir(WideToUTF8(full_path).c_str(), 0777) == 0);
}

bool GetFileSize(const std::wstring& file_path, int64* file_size) {
  struct stat64 file_info;
  if (stat64(WideToUTF8(file_path).c_str(), &file_info) != 0)
    return false;
  *file_size = file_info.st_size;
  return true;
}

int ReadFile(const std::wstring& filename, char* data, int size) {
  int fd = open(WideToUTF8(filename).c_str(), O_RDONLY);
  if (fd < 0)
    return -1;
  
  int ret_value = read(fd, data, size);
  close(fd);
  return ret_value;
}

int WriteFile(const std::wstring& filename, const char* data, int size) {
  int fd = open(WideToUTF8(filename).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 
                0666);
  if (fd < 0)
    return -1;
  
  int ret_value = write(fd, data, size);
  close(fd);
  return ret_value;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(std::wstring* dir) {
  char system_buffer[PATH_MAX] = "";
  getcwd(system_buffer, sizeof(system_buffer));
  *dir = UTF8ToWide(system_buffer);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const std::wstring& current_directory) {
  int ret = chdir(WideToUTF8(current_directory).c_str());
  return (ret == 0);
}
  
// TODO(erikkay): implement
#if 0
FileEnumerator::FileEnumerator(const std::string& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      find_handle_(INVALID_HANDLE_VALUE) {
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const std::string& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type,
                               const std::string& pattern)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      pattern_(pattern),
      find_handle_(INVALID_HANDLE_VALUE) {
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
  if (find_handle_ != INVALID_HANDLE_VALUE)
    FindClose(find_handle_);
}

std::wstring FileEnumerator::Next() {
  if (!is_in_find_op_) {
    if (pending_paths_.empty())
      return std::wstring();
    
    // The last find FindFirstFile operation is done, prepare a new one.
    // root_path_ must have the trailing directory character.
    root_path_ = pending_paths_.top();
    file_util::AppendToPath(&root_path_, std::wstring());
    pending_paths_.pop();
    
    // Start a new find operation.
    std::wstring src(root_path_);
    
    if (pattern_.empty())
      file_util::AppendToPath(&src, "*");  // No pattern = match everything.
    else
      file_util::AppendToPath(&src, pattern_);
    
    find_handle_ = FindFirstFile(src.c_str(), &find_data_);
    is_in_find_op_ = true;
    
  } else {
    // Search for the next file/directory.
    if (!FindNextFile(find_handle_, &find_data_)) {
      FindClose(find_handle_);
      find_handle_ = INVALID_HANDLE_VALUE;
    }
  }
  
  if (INVALID_HANDLE_VALUE == find_handle_) {
    is_in_find_op_ = false;
    
    // This is reached when we have finished a directory and are advancing to
    // the next one in the queue. We applied the pattern (if any) to the files
    // in the root search directory, but for those directories which were
    // matched, we want to enumerate all files inside them. This will happen
    // when the handle is empty.
    pattern_.clear();
    
    return Next();
  }
  
  std::wstring cur_file(find_data_.cFileName);
  // Skip over . and ..
  if (L"." == cur_file || L".." == cur_file)
    return Next();
  
  // Construct the absolute filename.
  cur_file.insert(0, root_path_);
  
  if (recursive_ && find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    // If |cur_file| is a directory, and we are doing recursive searching, add
    // it to pending_paths_ so we scan it after we finish scanning this
    // directory.
    pending_paths_.push(cur_file);
    return (file_type_ & FileEnumerator::DIRECTORIES) ? cur_file : Next();
  }
  return (file_type_ & FileEnumerator::FILES) ? cur_file : Next();
}
#endif
  
  
} // namespace file_util

