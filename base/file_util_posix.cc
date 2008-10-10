// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <fts.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>

#include <fstream>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace file_util {

static const wchar_t* kTempFileName = L"com.google.chrome.XXXXXX";

std::wstring GetDirectoryFromPath(const std::wstring& path) {
  if (EndsWithSeparator(path)) {
    std::wstring dir = path;
    TrimTrailingSeparator(&dir);
    return dir;
  } else {
    char full_path[PATH_MAX];
    base::strlcpy(full_path, WideToUTF8(path).c_str(), arraysize(full_path));
    return UTF8ToWide(dirname(full_path));
  }
}
  
bool AbsolutePath(std::wstring* path) {
  char full_path[PATH_MAX];
  if (realpath(WideToUTF8(*path).c_str(), full_path) == NULL)
    return false;
  *path = UTF8ToWide(full_path);
  return true;
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool Delete(const std::wstring& path, bool recursive) {
  std::string utf8_path_string = WideToUTF8(path);
  const char* utf8_path = utf8_path_string.c_str();
  struct stat64 file_info;
  int test = stat64(utf8_path, &file_info);
  if (test != 0) {
    // The Windows version defines this condition as success.
    bool ret = (errno == ENOENT || errno == ENOTDIR); 
    return ret;
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(utf8_path) == 0);
  if (!recursive)
    return (rmdir(utf8_path) == 0);

  bool success = true;
  int ftsflags = FTS_PHYSICAL | FTS_NOSTAT;
  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, utf8_path,
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }
  char* dir_list[2] = { top_dir, NULL };
  FTS* fts = fts_open(dir_list, ftsflags, NULL);
  if (fts) {
    FTSENT* fts_ent = fts_read(fts);
    while (success && fts_ent != NULL) {
      switch (fts_ent->fts_info) {
        case FTS_DNR:
        case FTS_ERR:
          // log error
          success = false;
          continue;
          break;
        case FTS_DP:
          rmdir(fts_ent->fts_accpath);
          break;
        case FTS_D:
          break;
        case FTS_NSOK:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
          unlink(fts_ent->fts_accpath);
          break;
        default:
          DCHECK(false);
          break;
      }
      fts_ent = fts_read(fts);
    }
    fts_close(fts);
  }
  return success;
}

bool Move(const std::wstring& from_path, const std::wstring& to_path) {
  return (rename(WideToUTF8(from_path).c_str(),
                 WideToUTF8(to_path).c_str()) == 0);
}

bool CopyDirectory(const std::wstring& from_path_wide,
                   const std::wstring& to_path_wide,
                   bool recursive) {
  const std::string to_path = WideToUTF8(to_path_wide);
  const std::string from_path = WideToUTF8(from_path_wide);

  // Some old callers of CopyDirectory want it to support wildcards.
  // After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  // TODO(evanm): remove this once we're sure it's ok.
  DCHECK(to_path.find('*') == std::string::npos);
  DCHECK(from_path.find('*') == std::string::npos);

  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, from_path.c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  char* dir_list[] = { top_dir, NULL };
  FTS* fts = fts_open(dir_list, FTS_PHYSICAL | FTS_NOSTAT, NULL);
  if (!fts) {
    LOG(ERROR) << "fts_open failed: " << strerror(errno);
    return false;
  }

  int error = 0;
  FTSENT* ent;
  while (!error && (ent = fts_read(fts)) != NULL) {
    // ent->fts_path is the source path, including from_path, so paste
    // the suffix after from_path onto to_path to create the target_path.
    const std::string target_path = to_path + &ent->fts_path[from_path.size()];
    switch (ent->fts_info) {
      case FTS_D:  // Preorder directory.
        // If we encounter a subdirectory in a non-recursive copy, prune it
        // from the traversal.
        if (!recursive && ent->fts_level > 0) {
          if (fts_set(fts, ent, FTS_SKIP) != 0)
            error = errno;
          continue;
        }

        // Try creating the target dir, continuing on it if it exists already.
        if (mkdir(target_path.c_str(), 0777) != 0) {
          if (errno != EEXIST)
            error = errno;
        }
        break;
      case FTS_F:     // Regular file.
      case FTS_NSOK:  // File, no stat info requested.
        // TODO(port): use a native file path rather than all these
        // conversions.
        errno = 0;
        if (!CopyFile(UTF8ToWide(ent->fts_path), UTF8ToWide(target_path)))
          error = errno ? errno : EINVAL;
        break;
      case FTS_DP:   // Postorder directory.
      case FTS_DOT:  // "." or ".."
        // Skip it.
        continue;
      case FTS_DC:   // Directory causing a cycle.
        // Skip this branch.
        if (fts_set(fts, ent, FTS_SKIP) != 0)
          error = errno;
        break;
      case FTS_DNR:  // Directory cannot be read.
      case FTS_ERR:  // Error.
      case FTS_NS:   // Stat failed.
        // Abort with the error.
        error = ent->fts_errno;
        break;
      case FTS_SL:      // Symlink.
      case FTS_SLNONE:  // Symlink with broken target.
        LOG(WARNING) << "CopyDirectory() skipping symbolic link.";
        continue;
      case FTS_DEFAULT:  // Some other sort of file.
        LOG(WARNING) << "CopyDirectory() skipping weird file.";
        continue;
      default:
        NOTREACHED();
        continue;  // Hope for the best!
    }
  }
  // fts_read may have returned NULL and set errno to indicate an error.
  if (!error && errno != 0)
    error = errno;

  if (!fts_close(fts)) {
    // If we already have an error, let's use that error instead of the error
    // fts_close set.
    if (!error)
      error = errno;
  }

  if (error) {
    LOG(ERROR) << "CopyDirectory(): " << strerror(error);
    return false;
  }
  return true;
}

bool PathExists(const std::wstring& path) {
  struct stat64 file_info;
  return (stat64(WideToUTF8(path).c_str(), &file_info) == 0);
}

bool DirectoryExists(const std::wstring& path) {
  struct stat64 file_info;
  if (stat64(WideToUTF8(path).c_str(), &file_info) == 0)
    return S_ISDIR(file_info.st_mode);
  return false;
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

bool CreateTemporaryFileName(std::wstring* temp_file) {
  std::wstring tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  AppendToPath(&tmpdir, kTempFileName);
  std::string tmpdir_string = WideToUTF8(tmpdir);
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());
  int fd = mkstemp(buffer);
  if (fd < 0)
    return false;
  *temp_file = UTF8ToWide(buffer);
  close(fd); 
  return true;
}

bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file) {
  // Not implemented yet.
  NOTREACHED();
  return false;
}

bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path) {
  std::wstring tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  AppendToPath(&tmpdir, kTempFileName);
  std::string tmpdir_string = WideToUTF8(tmpdir);
  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp)
    return false;
  *new_temp_path = UTF8ToWide(dtemp);
  return true;
}

bool CreateDirectory(const std::wstring& full_path) {
  std::vector<std::wstring> components;
  PathComponents(full_path, &components);
  std::wstring path;
  std::vector<std::wstring>::iterator i = components.begin();
  for (; i != components.end(); ++i) {
    if (path.length() == 0)
      path = *i;
    else
      AppendToPath(&path, *i);
    if (!DirectoryExists(path)) {
      if (mkdir(WideToUTF8(path).c_str(), 0777) != 0)
        return false;
    }
  }
  return true;
}

bool GetFileInfo(const std::wstring& file_path, FileInfo* results) {
  struct stat64 file_info;
  if (stat64(WideToUTF8(file_path).c_str(), &file_info) != 0)
    return false;
  results->is_directory = S_ISDIR(file_info.st_mode);
  results->size = file_info.st_size;
  return true;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  return fopen(filename.c_str(), mode);
}

FILE* OpenFile(const std::wstring& filename, const char* mode) {
  return fopen(WideToUTF8(filename).c_str(), mode);
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
  int fd = creat(WideToUTF8(filename).c_str(), 0666);
  if (fd < 0)
    return -1;

  // Allow for partial writes
  ssize_t bytes_written_total = 0;
  do {
    ssize_t bytes_written_partial = write(fd,
                                          data + bytes_written_total,
                                          size - bytes_written_total);
    if (bytes_written_partial < 0) {
      close(fd);
      return -1;      
    }
    bytes_written_total += bytes_written_partial;
  } while (bytes_written_total < size);

  close(fd);
  return bytes_written_total;
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
  
FileEnumerator::FileEnumerator(const std::wstring& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      fts_(NULL) {
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const std::wstring& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type,
                               const std::wstring& pattern)
    : recursive_(recursive),
      file_type_(file_type),
      pattern_(root_path),
      is_in_find_op_(false),
      fts_(NULL) {
  // The Windows version of this code only matches against items in the top-most
  // directory, and we're comparing fnmatch against full paths, so this is the
  // easiest way to get the right pattern.
  AppendToPath(&pattern_, pattern);
  pending_paths_.push(root_path);
}
  
FileEnumerator::~FileEnumerator() {
  if (fts_)
    fts_close(fts_);
}

// As it stands, this method calls itself recursively when the next item of
// the fts enumeration doesn't match (type, pattern, etc.).  In the case of
// large directories with many files this can be quite deep.
// TODO(erikkay) - get rid of this recursive pattern
std::wstring FileEnumerator::Next() {
  if (!is_in_find_op_) {
    if (pending_paths_.empty())
      return std::wstring();
    
    // The last find FindFirstFile operation is done, prepare a new one.
    root_path_ = pending_paths_.top();
    TrimTrailingSeparator(&root_path_);
    pending_paths_.pop();
    
    // Start a new find operation.
    int ftsflags = FTS_LOGICAL;
    char top_dir[PATH_MAX];
    base::strlcpy(top_dir, WideToUTF8(root_path_).c_str(), sizeof(top_dir));
    char* dir_list[2] = { top_dir, NULL };
    fts_ = fts_open(dir_list, ftsflags, NULL);
    if (!fts_)
      return Next();
    is_in_find_op_ = true;
  }
  
  FTSENT* fts_ent = fts_read(fts_);
  if (fts_ent == NULL) {
    fts_close(fts_);
    fts_ = NULL;
    is_in_find_op_ = false;
    return Next();
  }
  
  // Level 0 is the top, which is always skipped.
  if (fts_ent->fts_level == 0)
    return Next();
  
  // Patterns are only matched on the items in the top-most directory.
  // (see Windows implementation)
  if (fts_ent->fts_level == 1 && pattern_.length() > 0) {
    if (fnmatch(WideToUTF8(pattern_).c_str(), fts_ent->fts_path, 0) != 0) {
      if (fts_ent->fts_info == FTS_D)
        fts_set(fts_, fts_ent, FTS_SKIP);
      return Next();
    }
  }
  
  std::wstring cur_file(UTF8ToWide(fts_ent->fts_path));
  if (fts_ent->fts_info == FTS_D) {
    // If not recursive, then prune children.
    if (!recursive_)
      fts_set(fts_, fts_ent, FTS_SKIP);
    return (file_type_ & FileEnumerator::DIRECTORIES) ? cur_file : Next();
  } else if (fts_ent->fts_info == FTS_F) {
    return (file_type_ & FileEnumerator::FILES) ? cur_file : Next();
  }
  // TODO(erikkay) - verify that the other fts_info types aren't interesting
  return Next();
}
  
  
} // namespace file_util
