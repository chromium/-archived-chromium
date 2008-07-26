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

#include "chrome/browser/save_file.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/save_types.h"
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
