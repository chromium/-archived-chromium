// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/logging_installer.h"

CreateDirWorkItem::~CreateDirWorkItem() {
}

CreateDirWorkItem::CreateDirWorkItem(const std::wstring& path)
    : path_(path),
      rollback_needed_(false) {
}

void CreateDirWorkItem::GetTopDirToCreate() {
  if (file_util::PathExists(path_)) {
    top_path_.clear();
    return;
  }

  std::wstring parent_dir(path_);
  do {
    top_path_.assign(parent_dir);
    file_util::UpOneDirectoryOrEmpty(&parent_dir);
  } while (!parent_dir.empty() && !file_util::PathExists(parent_dir));
  return;
}

bool CreateDirWorkItem::Do() {
  LOG(INFO) << "creating directory " << path_;
  GetTopDirToCreate();
  if (top_path_.empty())
    return true;

  LOG(INFO) << "top directory that needs to be created: " \
            << top_path_;
  bool result = file_util::CreateDirectory(path_);
  LOG(INFO) << "directory creation result: " << result;

  rollback_needed_ = true;

  return result;
}

void CreateDirWorkItem::Rollback() {
  if (!rollback_needed_)
    return;

  // Delete all the directories we created to rollback.
  // Note we can not recusively delete top_path_ since we don't want to
  // delete non-empty directory. (We may have created a shared directory).
  // Instead we walk through path_ to top_path_ and delete directories
  // along the way.
  std::wstring path_to_delete(path_);

  while(1) {
    if (file_util::PathExists(path_to_delete)) {
      if (!RemoveDirectory(path_to_delete.c_str()))
        break;
    }
    if (!path_to_delete.compare(top_path_))
      break;
    file_util::UpOneDirectoryOrEmpty(&path_to_delete);
  }

  return;
}
