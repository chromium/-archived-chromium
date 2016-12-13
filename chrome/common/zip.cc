// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/zip.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "net/base/file_stream.h"
#include "third_party/zlib/contrib/minizip/unzip.h"
#include "third_party/zlib/contrib/minizip/zip.h"
#if defined(OS_WIN)
#include "third_party/zlib/contrib/minizip/iowin32.h"
#endif

static const int kZipMaxPath = 256;
static const int kZipBufSize = 8192;

// Extract the 'current' selected file from the zip into dest_dir.
// Output filename is stored in out_file.  Returns true on success.
static bool ExtractCurrentFile(unzFile zip_file,
                               const FilePath& dest_dir) {
  char filename_inzip[kZipMaxPath] = {0};
  unz_file_info file_info;
  int err = unzGetCurrentFileInfo(zip_file, &file_info, filename_inzip,
                                  sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK)
    return false;
  if (filename_inzip[0] == '\0')
    return false;

  err = unzOpenCurrentFile(zip_file);
  if (err != UNZ_OK)
    return false;

  FilePath::StringType filename;
  std::vector<FilePath::StringType> filename_parts;
#if defined(OS_WIN)
  filename = UTF8ToWide(filename_inzip);
#elif defined(OS_POSIX)
  filename = filename_inzip;
#endif
  SplitString(filename, '/', &filename_parts);

  FilePath dest_file(dest_dir);
  std::vector<FilePath::StringType>::iterator iter;
  for (iter = filename_parts.begin(); iter != filename_parts.end(); ++iter)
    dest_file = dest_file.Append(*iter);

  // If this is a directory, just create it and return.
  if (filename_inzip[strlen(filename_inzip) - 1] == '/') {
    if (!file_util::CreateDirectory(dest_file))
      return false;
    return true;
  }

  // We can't rely on parent directory entries being specified in the zip, so we
  // make sure they are created.
  FilePath dir = dest_file.DirName();
  if (!file_util::CreateDirectory(dir))
    return false;

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(dest_file, flags) != 0)
    return false;

  bool ret = true;
  int num_bytes = 0;
  char buf[kZipBufSize];
  do {
    num_bytes = unzReadCurrentFile(zip_file, buf, kZipBufSize);
    if (num_bytes < 0) {
      // If num_bytes < 0, then it's a specific UNZ_* error code.
      // While we're not currently handling these codes specifically, save
      // it away in case we want to in the future.
      err = num_bytes;
      break;
    }
    if (num_bytes > 0) {
      if (num_bytes != stream.Write(buf, num_bytes, NULL)) {
        ret = false;
        break;
      }
    }
  } while (num_bytes > 0);

  stream.Close();
  if (err == UNZ_OK)
    err = unzCloseCurrentFile(zip_file);
  else
    unzCloseCurrentFile(zip_file);  // Don't lose the original error code.
  if (err != UNZ_OK)
    ret = false;
  return ret;
}

#if defined(OS_WIN)
typedef struct {
  HANDLE hf;
  int error;
} WIN32FILE_IOWIN;

// This function is derived from third_party/minizip/iowin32.c.
// Its only difference is that it treats the char* as UTF8 and
// uses the Unicode version of CreateFile.
static void* ZipOpenFunc(void *opaque, const char* filename, int mode) {
  DWORD desired_access, creation_disposition;
  DWORD share_mode, flags_and_attributes;
  HANDLE file = 0;
  void* ret = NULL;

  desired_access = share_mode = flags_and_attributes = 0;

  if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ) {
    desired_access = GENERIC_READ;
    creation_disposition = OPEN_EXISTING;
    share_mode = FILE_SHARE_READ;
  } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING) {
    desired_access = GENERIC_WRITE | GENERIC_READ;
    creation_disposition = OPEN_EXISTING;
  } else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
    desired_access = GENERIC_WRITE | GENERIC_READ;
    creation_disposition = CREATE_ALWAYS;
  }

  std::wstring filename_wstr = UTF8ToWide(filename);
  if ((filename != NULL) && (desired_access != 0)) {
    file = CreateFile(filename_wstr.c_str(), desired_access, share_mode,
        NULL, creation_disposition, flags_and_attributes, NULL);
  }

  if (file == INVALID_HANDLE_VALUE)
    file = NULL;

  if (file != NULL) {
    WIN32FILE_IOWIN file_ret;
    file_ret.hf = file;
    file_ret.error = 0;
    ret = malloc(sizeof(WIN32FILE_IOWIN));
    if (ret == NULL)
      CloseHandle(file);
    else
      *(static_cast<WIN32FILE_IOWIN*>(ret)) = file_ret;
  }
  return ret;
}
#endif

bool Unzip(const FilePath& src_file, const FilePath& dest_dir) {
#if defined(OS_WIN)
  zlib_filefunc_def zip_funcs;
  fill_win32_filefunc(&zip_funcs);
  zip_funcs.zopen_file = ZipOpenFunc;
#endif

#if defined(OS_POSIX)
  std::string src_file_str = src_file.value();
  unzFile zip_file = unzOpen(src_file_str.c_str());
#elif defined(OS_WIN)
  std::string src_file_str = WideToUTF8(src_file.value());
  unzFile zip_file = unzOpen2(src_file_str.c_str(), &zip_funcs);
#endif
  if (!zip_file) {
    LOG(WARNING) << "couldn't create file " << src_file_str;
    return false;
  }
  unz_global_info zip_info;
  int err;
  err = unzGetGlobalInfo(zip_file, &zip_info);
  if (err != UNZ_OK) {
    LOG(WARNING) << "couldn't open zip " << src_file_str;
    return false;
  }
  bool ret = true;
  for (unsigned int i = 0; i < zip_info.number_entry; ++i) {
    if (!ExtractCurrentFile(zip_file, dest_dir)) {
      ret = false;
      break;
    }

    if (i + 1 < zip_info.number_entry) {
      err = unzGoToNextFile(zip_file);
      if (err != UNZ_OK) {
        LOG(WARNING) << "error %d in unzGoToNextFile";
        ret = false;
        break;
      }
    }
  }
  unzClose(zip_file);
  return ret;
}

static bool AddFileToZip(zipFile zip_file, const FilePath& src_dir) {
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  if (stream.Open(src_dir, flags) != 0) {
    LOG(ERROR) << "Could not open stream for path "
               << WideToASCII(src_dir.ToWStringHack());
    return false;
  }

  int num_bytes;
  char buf[kZipBufSize];
  do {
    num_bytes = stream.Read(buf, kZipBufSize, NULL);
    if (num_bytes > 0) {
      if (ZIP_OK != zipWriteInFileInZip(zip_file, buf, num_bytes)) {
        LOG(ERROR) << "Could not write data to zip for path "
                   << WideToASCII(src_dir.ToWStringHack());
        return false;
      }
    }
  } while (num_bytes > 0);

  return true;
}

static bool AddEntryToZip(zipFile zip_file, const FilePath& path,
                          const FilePath& root_path) {
#if defined(OS_WIN)
  std::string str_path =
      WideToUTF8(path.value().substr(root_path.value().length() + 1));
  ReplaceSubstringsAfterOffset(&str_path, 0u, "\\", "/");
#else
  std::string str_path = path.value().substr(root_path.value().length() + 1);
#endif

  bool is_directory = file_util::DirectoryExists(path);
  if (is_directory)
    str_path += "/";

  if (ZIP_OK != zipOpenNewFileInZip(
      zip_file, str_path.c_str(),
      NULL, NULL, 0u, NULL, 0u, NULL,  // file info, extrafield local, length,
                                       // extrafield global, length, comment
      Z_DEFLATED, Z_DEFAULT_COMPRESSION)) {
    LOG(ERROR) << "Could not open zip file entry " << str_path;
    return false;
  }

  bool success = true;
  if (!is_directory) {
    success = AddFileToZip(zip_file, path);
  }

  if (ZIP_OK != zipCloseFileInZip(zip_file)) {
    LOG(ERROR) << "Could not close zip file entry " << str_path;
    return false;
  }

  return success;
}

bool Zip(const FilePath& src_dir, const FilePath& dest_file) {
  DCHECK(file_util::DirectoryExists(src_dir));

#if defined(OS_WIN)
  zlib_filefunc_def zip_funcs;
  fill_win32_filefunc(&zip_funcs);
  zip_funcs.zopen_file = ZipOpenFunc;
#endif

#if defined(OS_POSIX)
  std::string dest_file_str = dest_file.value();
  std::string src_dir_str = src_dir.value();
  zipFile zip_file = zipOpen(dest_file_str.c_str(), APPEND_STATUS_CREATE);
#elif defined(OS_WIN)
  std::string dest_file_str = WideToUTF8(dest_file.value());
  zipFile zip_file = zipOpen2(dest_file_str.c_str(), APPEND_STATUS_CREATE,
                              NULL,  // global comment
                              &zip_funcs);
#endif

  if (!zip_file) {
    LOG(WARNING) << "couldn't create file " << dest_file_str;
    return false;
  }

  bool success = true;
  file_util::FileEnumerator file_enumerator(
      src_dir, true,  // recursive
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES));
  for (FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    if (!AddEntryToZip(zip_file, path, src_dir)) {
      success = false;
      return false;
    }
  }

  if (ZIP_OK != zipClose(zip_file, NULL)) {  // global comment
    LOG(ERROR) << "Error closing zip file " << dest_file_str;
    return false;
  }

  return success;
}
