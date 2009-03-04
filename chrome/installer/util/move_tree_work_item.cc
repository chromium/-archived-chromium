// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/move_tree_work_item.h"

#include <shlwapi.h>
#include "base/file_util.h"
#include "chrome/installer/util/logging_installer.h"

MoveTreeWorkItem::~MoveTreeWorkItem() {
  if (file_util::PathExists(backup_path_)) {
    file_util::Delete(backup_path_, true);
  }
}

MoveTreeWorkItem::MoveTreeWorkItem(const std::wstring& source_path,
                                   const std::wstring& dest_path,
                                   const std::wstring& temp_dir)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_dir_(temp_dir),
      moved_to_dest_path_(false),
      moved_to_backup_(false) {
}

bool MoveTreeWorkItem::Do() {
  if (!file_util::PathExists(source_path_)) {
    LOG(ERROR) << source_path_ << " does not exist";
    return false;
  }

  // If dest_path_ exists, move destination to a backup path.
  if (file_util::PathExists(dest_path_)) {
    // Generate a backup path that can keep the original files under dest_path_.
    if (!file_util::CreateTemporaryFileNameInDir(temp_dir_, &backup_path_)) {
      LOG(ERROR) << "Failed to get backup path in folder " << temp_dir_;
      return false;
    }

    if (file_util::Move(dest_path_, backup_path_)) {
      moved_to_backup_ = true;
      LOG(INFO) << "Moved destination " << dest_path_
                << " to backup path " << backup_path_;
    } else {
      LOG(ERROR) << "failed moving " << dest_path_ << " to " << backup_path_;
      return false;
    }
  }

  // Now move source to destination.
  if (file_util::Move(source_path_, dest_path_)) {
    moved_to_dest_path_ = true;
    LOG(INFO) << "Moved source " << source_path_
              << " to destination " << dest_path_;
  } else {
    LOG(ERROR) << "failed move " << source_path_ << " to " << dest_path_;
    return false;
  }

  return true;
}

void MoveTreeWorkItem::Rollback() {
  if (moved_to_dest_path_ && !file_util::Move(dest_path_, source_path_))
    LOG(ERROR) << "Can not move " << dest_path_ << " to " << source_path_;

  if (moved_to_backup_ && !file_util::Move(backup_path_, dest_path_))
    LOG(ERROR) << "failed move " << backup_path_ << " to " << dest_path_;
}
