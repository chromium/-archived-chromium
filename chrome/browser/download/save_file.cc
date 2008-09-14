// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_file.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/download/save_types.h"
#include "chrome/common/win_util.h"
#include "chrome/common/win_safe_util.h"

SaveFile::SaveFile(const SaveFileCreateInfo* info)
    : info_(info),
      file_(NULL),
      bytes_so_far_(0),
      path_renamed_(false),
      in_progress_(true) {
  DCHECK(info);
  DCHECK(info->path.empty());
  if (file_util::CreateTemporaryFileName(&full_path_))
    Open(L"wb");
}

SaveFile::~SaveFile() {
  Close();
}

// Return false indicate that we got disk error, save file manager will tell
// SavePackage this error, then SavePackage will call its Cancel() method to
// cancel whole save job.
bool SaveFile::AppendDataToFile(const char* data, int data_len) {
  if (file_) {
    if (data_len == fwrite(data, 1, data_len, file_)) {
      bytes_so_far_ += data_len;
      return true;
    } else {
      Close();
      return false;
    }
  }
  // No file_, treat it as disk error.
  return false;
}

void SaveFile::Cancel() {
  Close();
  // If this job has been canceled, and it has created file,
  // We need to delete this created file.
  if (!full_path_.empty()) {
    DeleteFile(full_path_.c_str());
  }
}

// Rename the file when we have final name.
bool SaveFile::Rename(const std::wstring& new_path) {
  Close();

  DCHECK(!path_renamed());
  // We cannot rename because rename will keep the same security descriptor
  // on the destination file. We want to recreate the security descriptor
  // with the security that makes sense in the new path. If the last parameter
  // is FALSE and the new file already exists, the function overwrites the
  // existing file and succeeds.
  if (!CopyFile(full_path_.c_str(), new_path.c_str(), FALSE))
    return false;

  DeleteFile(full_path_.c_str());

  full_path_ = new_path;
  path_renamed_ = true;

  // Still in saving process, reopen the file.
  if (in_progress_ && !Open(L"a+b"))
    return false;
  return true;
}

void SaveFile::Finish() {
  Close();
  in_progress_ = false;
}

void SaveFile::Close() {
  if (file_) {
    fclose(file_);
    file_ = NULL;
  }
}

bool SaveFile::Open(const wchar_t* open_mode) {
  DCHECK(!full_path_.empty());
  if (_wfopen_s(&file_, full_path_.c_str(), open_mode)) {
    file_ = NULL;
    return false;
  }
  // Sets the zone to tell Windows that this file comes from the Internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_);
  return true;
}

