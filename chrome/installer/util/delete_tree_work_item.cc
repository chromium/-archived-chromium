// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

DeleteTreeWorkItem::DeleteTreeWorkItem(const std::wstring& root_path,
                                       const std::wstring& key_path)
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

bool DeleteTreeWorkItem::GetBackupPath(const std::wstring& for_path,
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
