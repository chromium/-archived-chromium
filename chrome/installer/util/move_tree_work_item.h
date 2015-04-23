// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_

#include <string>
#include <windows.h>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively move a file system hierarchy from
// source path to destination path. The file system hierarchy could be a
// single file, or a directory.
//
// Under the cover MoveTreeWorkItem moves the destination path, if existing,
// to the temporary directory passed in, and then moves the source hierarchy
// to the destination location. During rollback the original destination
// hierarchy is moved back.
class MoveTreeWorkItem : public WorkItem {
 public:
  virtual ~MoveTreeWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  // source_path specifies file or directory that will be moved to location
  // specified by dest_path. To facilitate rollback, the caller needs to supply
  // a temporary directory (temp_dir) to save the original files if they exist
  // under dest_path.
  MoveTreeWorkItem(const std::wstring& source_path,
                   const std::wstring& dest_path,
                   const std::wstring& temp_dir);

  // Source path to move files from.
  std::wstring source_path_;

  // Destination path to move files to.
  std::wstring dest_path_;

  // Temporary directory to backup dest_path_ (if it already exists).
  std::wstring temp_dir_;

  // The full path in temp_dir_ where the original dest_path_ has
  // been moved to.
  std::wstring backup_path_;

  // Whether the source was moved to dest_path_
  bool moved_to_dest_path_;

  // Whether the original files have been moved to backup path under
  // temporary directory. If true, moving back is needed during rollback.
  bool moved_to_backup_;
};

#endif  // CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
