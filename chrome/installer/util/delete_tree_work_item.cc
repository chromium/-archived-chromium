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
#include "base/logging.h"
#include "chrome/installer/util/delete_tree_work_item.h"

DeleteTreeWorkItem::~DeleteTreeWorkItem() {
  std::wstring tmp_dir = file_util::GetDirectoryFromPath(backup_path_);
  if (file_util::PathExists(tmp_dir)) {
    file_util::Delete(tmp_dir, true);
  }
  tmp_dir = file_util::GetDirectoryFromPath(key_backup_path_);
  if (file_util::PathExists(tmp_dir)) {
    file_util::Delete(tmp_dir, true);
  }
}

DeleteTreeWorkItem::DeleteTreeWorkItem(std::wstring root_path,
                                       std::wstring key_path)
    : root_path_(root_path),
      key_path_(key_path) {
}

// We first try to move key_path_ to backup_path. If it succeeds, we go ahead
// and move the rest.
bool DeleteTreeWorkItem::Do() {
  if (!key_path_.empty() && file_util::PathExists(key_path_)) {
    if (!GetBackupPath(key_path_, &key_backup_path_) ||
        !file_util::CopyDirectory(key_path_, key_backup_path_, true) ||
        !file_util::Delete(key_path_, true)) {
      LOG(ERROR) << "can not delete " << key_path_
                 << " OR copy it to backup path " << key_backup_path_;
      return false;
    }
  }

  if (!root_path_.empty() && file_util::PathExists(root_path_)) {
    if (!GetBackupPath(root_path_, &backup_path_) ||
        !file_util::CopyDirectory(root_path_, backup_path_, true) ||
        !file_util::Delete(root_path_, true)) {
      LOG(ERROR) << "can not delete " << root_path_
                 << " OR copy it to backup path " << backup_path_;
      return false;
    }
  }
  return true;
}

// If there are files in backup paths move them back.
void DeleteTreeWorkItem::Rollback() {
  if (!backup_path_.empty() && file_util::PathExists(backup_path_)) {
    file_util::Move(backup_path_, root_path_);
  }
  if (!key_backup_path_.empty() && file_util::PathExists(key_backup_path_)) {
    file_util::Move(key_backup_path_, key_path_);
  }
}

bool DeleteTreeWorkItem::GetBackupPath(std::wstring for_path,
                                       std::wstring* backup_path) {
  if (!file_util::CreateNewTempDirectory(L"", backup_path)) {
    // We assume that CreateNewTempDirectory() is doing its job well.
    LOG(ERROR) << "Couldn't get backup path for delete.";
    return false;
  }
  std::wstring file_name = file_util::GetFilenameFromPath(for_path);
  file_util::AppendToPath(backup_path, file_name);

  return true;
}
