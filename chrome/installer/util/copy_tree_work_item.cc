// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

#include <shlwapi.h>
#include "base/file_util.h"
#include "chrome/installer/util/logging_installer.h"

CopyTreeWorkItem::~CopyTreeWorkItem() {
  if (file_util::PathExists(backup_path_)) {
    file_util::Delete(backup_path_, true);
  }
}

CopyTreeWorkItem::CopyTreeWorkItem(const std::wstring& source_path,
                                   const std::wstring& dest_path,
                                   const std::wstring& temp_dir,
                                   CopyOverWriteOption overwrite_option,
                                   const std::wstring& alternative_path)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_dir_(temp_dir),
      overwrite_option_(overwrite_option),
      alternative_path_(alternative_path),
      copied_to_dest_path_(false),
      moved_to_backup_(false),
      copied_to_alternate_path_(false) {
}

bool CopyTreeWorkItem::Do() {
  if (!file_util::PathExists(source_path_)) {
    LOG(ERROR) << source_path_ << " does not exist";
    return false;
  }

  bool dest_exist = file_util::PathExists(dest_path_);
  // handle overwrite_option_ = IF_DIFFERENT case.
  if ((dest_exist) &&
      (overwrite_option_ == WorkItem::IF_DIFFERENT) && // only for single file
      (!PathIsDirectory(source_path_.c_str())) &&
      (!PathIsDirectory(dest_path_.c_str())) &&
      (file_util::ContentsEqual(source_path_, dest_path_))) {
    LOG(INFO) << "Source file " << source_path_
              << " and destination file " << dest_path_
              << " are exactly same. Returning true.";
    return true;
  } else if ((dest_exist) &&
             (overwrite_option_ == WorkItem::NEW_NAME_IF_IN_USE) &&
             (!PathIsDirectory(source_path_.c_str())) &&
             (!PathIsDirectory(dest_path_.c_str())) &&
             (IsFileInUse(dest_path_))) {
    // handle overwrite_option_ = NEW_NAME_IF_IN_USE case.
    if (alternative_path_.empty() ||
        file_util::PathExists(alternative_path_) ||
        !file_util::CopyFile(source_path_, alternative_path_)) {
      LOG(ERROR) << "failed to copy " << source_path_ <<
                    " to " << alternative_path_;
      return false;
    } else {
      copied_to_alternate_path_ = true;
      LOG(INFO) << "Copied source file " << source_path_
                << " to alternative path " << alternative_path_;
      return true;
    }
  } else if ((dest_exist) &&
             (overwrite_option_ == WorkItem::IF_NOT_PRESENT)) {
    // handle overwrite_option_ = IF_NOT_PRESENT case.
    return true;
  }

  // In all cases that reach here, move dest to a backup path.
  if (dest_exist) {
    if (!GetBackupPath())
      return false;

    if (file_util::Move(dest_path_, backup_path_)) {
      moved_to_backup_ = true;
      LOG(INFO) << "Moved destination " << dest_path_
                << " to backup path " << backup_path_;
    } else {
      LOG(ERROR) << "failed moving " << dest_path_ << " to " << backup_path_;
      return false;
    }
  }

  // In all cases that reach here, copy source to destination.
  if (file_util::CopyDirectory(source_path_, dest_path_, true)) {
    copied_to_dest_path_ = true;
    LOG(INFO) << "Copied source " << source_path_
              << " to destination " << dest_path_;
  } else {
    LOG(ERROR) << "failed copy " << source_path_ << " to " << dest_path_;
    return false;
  }

  return true;
}

void CopyTreeWorkItem::Rollback() {
  // Normally the delete operations below should not fail unless some
  // programs like anti-virus are inpecting the files we just copied.
  // If this does happen sometimes, we may consider using Move instead of
  // Delete here. For now we just log the error and continue with the
  // rest of rollback operation.
  if (copied_to_dest_path_ && !file_util::Delete(dest_path_, true)) {
    LOG(ERROR) << "Can not delete " << dest_path_;
  }
  if (moved_to_backup_ && !file_util::Move(backup_path_, dest_path_)) {
    LOG(ERROR) << "failed move " << backup_path_ << " to " << dest_path_;
  }
  if (copied_to_alternate_path_ &&
      !file_util::Delete(alternative_path_, true)) {
    LOG(ERROR) << "Can not delete " << alternative_path_;
  }
}

bool CopyTreeWorkItem::IsFileInUse(const std::wstring& path) {
  if (!file_util::PathExists(path))
    return false;

  HANDLE handle = ::CreateFile(path.c_str(), FILE_ALL_ACCESS,
                               NULL, NULL, OPEN_EXISTING, NULL, NULL);
  if (handle  == INVALID_HANDLE_VALUE)
    return true;

  CloseHandle(handle);
  return false;
}

bool CopyTreeWorkItem::GetBackupPath() {
  std::wstring file_name = file_util::GetFilenameFromPath(dest_path_);
  backup_path_.assign(temp_dir_);
  file_util::AppendToPath(&backup_path_, file_name);

  if (file_util::PathExists(backup_path_)) {
    // Ideally we should not fail immediately. Instead we could try some
    // random paths under temp_dir_ until we reach certain limit.
    // For now our caller always provides a good temporary directory so
    // we don't bother.
    LOG(ERROR) << "backup path " << backup_path_ << " already exists";
    return false;
  }

  return true;
}
