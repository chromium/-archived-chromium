// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>
#include <string>

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "base/win_util.h"

namespace file_util {

const wchar_t kPathSeparator = L'\\';

std::wstring GetDirectoryFromPath(const std::wstring& path) {
  wchar_t path_buffer[MAX_PATH];
  wchar_t* file_ptr = NULL;
  if (GetFullPathName(path.c_str(), MAX_PATH, path_buffer, &file_ptr) == 0)
    return L"";

  std::wstring::size_type length =
      file_ptr ? file_ptr - path_buffer : path.length();
  std::wstring directory(path, 0, length);
  TrimTrailingSeparator(&directory);
  return directory;
}

bool AbsolutePath(std::wstring* path) {
  wchar_t file_path_buf[MAX_PATH];
  if (!_wfullpath(file_path_buf, path->c_str(), MAX_PATH))
    return false;
  *path = file_path_buf;
  return true;
}
  
int CountFilesCreatedAfter(const std::wstring& path,
                           const FILETIME& comparison_time) {
  int file_count = 0;

  WIN32_FIND_DATA find_file_data;
  std::wstring filename_spec = path + L"\\*";  // All files in given dir
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      // Don't count current or parent directories.
      if ((wcscmp(find_file_data.cFileName, L"..") == 0) ||
          (wcscmp(find_file_data.cFileName, L".") == 0))
        continue;

      long result = CompareFileTime(&find_file_data.ftCreationTime,
                                    &comparison_time);
      // File was created after or on comparison time
      if ((result == 1) || (result == 0))
        ++file_count;
    } while (FindNextFile(find_handle,  &find_file_data));
    FindClose(find_handle);
  }

  return file_count;
}

bool Delete(const std::wstring& path, bool recursive) {
  if (path.length() >= MAX_PATH)
    return false;

  // If we're not recursing use DeleteFile; it should be faster. DeleteFile
  // fails if passed a directory though, which is why we fall through on
  // failure to the SHFileOperation.
  if (!recursive && DeleteFile(path.c_str()) != 0)
    return true;

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path, path.c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;
  return (SHFileOperation(&file_operation) == 0);
}

bool Move(const std::wstring& from_path, const std::wstring& to_path) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.length() >= MAX_PATH || to_path.length() >= MAX_PATH)
    return false;
  return (MoveFileEx(from_path.c_str(), to_path.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0);
}

bool CopyFile(const std::wstring& from_path, const std::wstring& to_path) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.length() >= MAX_PATH || to_path.length() >= MAX_PATH)
    return false;
  return (::CopyFile(from_path.c_str(), to_path.c_str(), false) != 0);
}

bool ShellCopy(const std::wstring& from_path, const std::wstring& to_path,
               bool recursive) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.length() >= MAX_PATH || to_path.length() >= MAX_PATH)
    return false;

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path_from[MAX_PATH + 1] = {0};
  wchar_t double_terminated_path_to[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_from, from_path.c_str());
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_to, to_path.c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_COPY;
  file_operation.pFrom = double_terminated_path_from;
  file_operation.pTo = double_terminated_path_to;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION |
                          FOF_NOCONFIRMMKDIR;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;

  return (SHFileOperation(&file_operation) == 0);
}

bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive) {
  if (recursive)
    return ShellCopy(from_path, to_path, true);

  // Instead of creating a new directory, we copy the old one to include the
  // security information of the folder as part of the copy.
  if (!PathExists(to_path)) {
    // Except that Vista fails to do that, and instead do a recursive copy if
    // the target directory doesn't exist.
    if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
      CreateDirectory(to_path);
    else
      ShellCopy(from_path, to_path, false);
  }

  std::wstring directory(from_path);
  AppendToPath(&directory, L"*.*");
  return ShellCopy(directory, to_path, false);
}

bool PathExists(const std::wstring& path) {
  return (GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const std::wstring& path) {
  HANDLE dir =
      CreateFile(path.c_str(), FILE_ADD_FILE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(dir);
  return true;
}

bool DirectoryExists(const std::wstring& path) {
  DWORD fileattr = GetFileAttributes(path.c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
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

bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time) {
  ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}

bool ResolveShortcut(std::wstring* path) {
  HRESULT result;
  IShellLink *shell = NULL;
  bool is_resolved = false;

  // Get pointer to the IShellLink interface
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                            CLSCTX_INPROC_SERVER, IID_IShellLink,
                            reinterpret_cast<LPVOID*>(&shell));
  if (SUCCEEDED(result)) {
    IPersistFile *persist = NULL;
    // Query IShellLink for the IPersistFile interface
    result = shell->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<LPVOID*>(&persist));
    if (SUCCEEDED(result)) {
      WCHAR temp_path[MAX_PATH];
      // Load the shell link
      result = persist->Load(path->c_str(), STGM_READ);
      if (SUCCEEDED(result)) {
        // Try to find the target of a shortcut
        result = shell->Resolve(0, SLR_NO_UI);
        if (SUCCEEDED(result)) {
          result = shell->GetPath(temp_path, MAX_PATH,
                                  NULL, SLGP_UNCPRIORITY);
          *path = temp_path;
          is_resolved = true;
        }
      }
    }
    if (persist)
      persist->Release();
  }
  if (shell)
    shell->Release();

  return is_resolved;
}

bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index) {
  IShellLink *i_shell_link = NULL;
  IPersistFile *i_persist_file = NULL;

  // Get pointer to the IShellLink interface
  HRESULT result = CoCreateInstance(CLSID_ShellLink, NULL,
                                    CLSCTX_INPROC_SERVER, IID_IShellLink,
                                    reinterpret_cast<LPVOID*>(&i_shell_link));
  if (FAILED(result))
    return false;

  // Query IShellLink for the IPersistFile interface
  result = i_shell_link->QueryInterface(IID_IPersistFile,
      reinterpret_cast<LPVOID*>(&i_persist_file));
  if (FAILED(result)) {
    i_shell_link->Release();
    return false;
  }

  if (FAILED(i_shell_link->SetPath(source))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (arguments && FAILED(i_shell_link->SetArguments(arguments))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (description && FAILED(i_shell_link->SetDescription(description))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  result = i_persist_file->Save(destination, TRUE);
  i_persist_file->Release();
  i_shell_link->Release();
  return SUCCEEDED(result);
}


bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index) {
  // Get pointer to the IPersistFile interface and load existing link
  IShellLink *i_shell_link = NULL;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL,
                              CLSCTX_INPROC_SERVER, IID_IShellLink,
                              reinterpret_cast<LPVOID*>(&i_shell_link))))
    return false;

  IPersistFile *i_persist_file = NULL;
  if (FAILED(i_shell_link->QueryInterface(
      IID_IPersistFile, reinterpret_cast<LPVOID*>(&i_persist_file)))) {
    i_shell_link->Release();
    return false;
  }

  if (FAILED(i_persist_file->Load(destination, 0))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (source && FAILED(i_shell_link->SetPath(source))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (arguments && FAILED(i_shell_link->SetArguments(arguments))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (description && FAILED(i_shell_link->SetDescription(description))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  HRESULT result = i_persist_file->Save(destination, TRUE);
  i_persist_file->Release();
  i_shell_link->Release();
  return SUCCEEDED(result);
}

bool GetTempDir(std::wstring* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  path->assign(temp_path);
  TrimTrailingSeparator(path);
  return true;
}

bool CreateTemporaryFileName(std::wstring* temp_file) {
  wchar_t temp_name[MAX_PATH + 1];
  std::wstring temp_path;

  if (!GetTempDir(&temp_path))
    return false;

  if (!GetTempFileName(temp_path.c_str(), L"", 0, temp_name))
    return false;  // fail!

  DWORD path_len = GetLongPathName(temp_name, temp_name, MAX_PATH);
  if (path_len > MAX_PATH + 1 || path_len == 0)
    return false;  // fail!

  temp_file->assign(temp_name, path_len);
  return true;
}

bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path) {
  std::wstring system_temp_dir;
  if (!GetTempDir(&system_temp_dir))
    return false;

  std::wstring path_to_create;
  srand(static_cast<uint32>(time(NULL)));

  int count = 0;
  while (count < 50) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    path_to_create.assign(system_temp_dir);
    std::wstring new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(IntToWString(rand() % kint16max));
    file_util::AppendToPath(&path_to_create, new_dir_name);

    if (::CreateDirectory(path_to_create.c_str(), NULL))
      break;
    count++;
  }

  if (count == 50) {
    return false;
  }

  new_temp_path->assign(path_to_create);
  return true;
}

bool CreateDirectory(const std::wstring& full_path) {
  if (DirectoryExists(full_path))
    return true;
  int err = SHCreateDirectoryEx(NULL, full_path.c_str(), NULL);
  return err == ERROR_SUCCESS;
}

bool GetFileInfo(const std::wstring& file_path, FileInfo* results) {
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.c_str(), GetFileExInfoStandard, &attr))
    return false;

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  results->size = size.QuadPart;

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return true;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file;
  if (fopen_s(&file, filename.c_str(), mode) != 0) {
    return NULL;
  }
  return file;
}

FILE* OpenFile(const std::wstring& filename, const char* mode) {
  std::wstring w_mode = ASCIIToWide(std::string(mode));
  FILE* file;
  if (_wfopen_s(&file, filename.c_str(), w_mode.c_str()) != 0) {
    return NULL;
  }
  return file;
}

int ReadFile(const std::wstring& filename, char* data, int size) {
  ScopedHandle file(CreateFile(filename.c_str(),
                               GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL));
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  int ret_value;
  DWORD read;
  if (::ReadFile(file, data, size, &read, NULL) && read == size) {
    ret_value = static_cast<int>(read);
  } else {
    ret_value = -1;
  }

  return ret_value;
}

int WriteFile(const std::wstring& filename, const char* data, int size) {
  ScopedHandle file(CreateFile(filename.c_str(),
                               GENERIC_WRITE,
                               0,
                               NULL,
                               CREATE_ALWAYS,
                               0,
                               NULL));
  if (file == INVALID_HANDLE_VALUE) {
    LOG(WARNING) << "CreateFile failed for path " << filename <<
        " error code=" << GetLastError() <<
        " error text=" << win_util::FormatLastWin32Error();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && written == size)
    return static_cast<int>(written);

  if (!result) {
    // WriteFile failed.
    LOG(WARNING) << "writing file " << filename <<
        " failed, error code=" << GetLastError() <<
        " description=" << win_util::FormatLastWin32Error();
  } else {
    // Didn't write all the bytes.
    LOG(WARNING) << "wrote" << written << " bytes to " << filename <<
        " expected " << size;
  }
  return -1;
}

bool RenameFileAndResetSecurityDescriptor(
    const std::wstring& source_file_path,
    const std::wstring& target_file_path) {
  // The MoveFile API does not reset the security descriptor on the target
  // file. To ensure that the target file gets the correct security descriptor
  // we create the target file initially in the target path, read its security
  // descriptor and stamp this descriptor on the target file after the MoveFile
  // API completes.
  ScopedHandle temp_file_handle_for_security_desc(
      CreateFileW(target_file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                  FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                  NULL));
  if (!temp_file_handle_for_security_desc.IsValid())
    return false;

  // Check how much we should allocate for the security descriptor.
  unsigned long security_descriptor_size_in_bytes = 0;
  GetFileSecurity(target_file_path.c_str(), DACL_SECURITY_INFORMATION, NULL, 0,
                  &security_descriptor_size_in_bytes);
  if (ERROR_INSUFFICIENT_BUFFER != GetLastError() ||
      security_descriptor_size_in_bytes == 0)
    return false;

  scoped_array<char> security_descriptor(
      new char[security_descriptor_size_in_bytes]);

  if (!GetFileSecurity(target_file_path.c_str(), DACL_SECURITY_INFORMATION,
                       security_descriptor.get(),
                       security_descriptor_size_in_bytes,
                       &security_descriptor_size_in_bytes)) {
    return false;
  }

  temp_file_handle_for_security_desc.Set(INVALID_HANDLE_VALUE);

  if (!MoveFileEx(source_file_path.c_str(), target_file_path.c_str(),
                  MOVEFILE_COPY_ALLOWED)) {
    return false;
  }

  return !!SetFileSecurity(target_file_path.c_str(),
                           DACL_SECURITY_INFORMATION,
                           security_descriptor.get());
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(std::wstring* dir) {
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  *dir = system_buffer;
  file_util::TrimTrailingSeparator(dir);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const std::wstring& current_directory) {
  BOOL ret = ::SetCurrentDirectory(current_directory.c_str());
  return (ret ? true : false);
}

///////////////////////////////////////////////


FileEnumerator::FileEnumerator(const std::wstring& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      find_handle_(INVALID_HANDLE_VALUE) {
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const std::wstring& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type,
                               const std::wstring& pattern)
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
      file_util::AppendToPath(&src, L"*");  // No pattern = match everything.
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

  if (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    if (recursive_) {
      // If |cur_file| is a directory, and we are doing recursive searching, add
      // it to pending_paths_ so we scan it after we finish scanning this
      // directory.
      pending_paths_.push(cur_file);
      return (file_type_ & FileEnumerator::DIRECTORIES) ? cur_file : Next();
    }

    if ((file_type_ & FileEnumerator::DIRECTORIES) == 0)
      return Next();
  }
  return (file_type_ & FileEnumerator::FILES) ? cur_file : Next();
}

}  // namespace file_util
