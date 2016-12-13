// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_

#include <string>
#include <windows.h>
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively copies a file system hierarchy from
// source path to destination path. It also creates all necessary intermediate
// paths of the destination path if they do not exist. The file system
// hierarchy could be a single file, or a directory.
// Under the cover CopyTreeWorkItem moves the destination path, if existing,
// to the temporary directory passed in, and then copies the source hierarchy
// to the destination location. During rollback the original destination
// hierarchy is moved back.
class CopyTreeWorkItem : public WorkItem {
 public:
  virtual ~CopyTreeWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  // See comments on corresponding member varibles for the semantics of
  // arguments.
  // Notes on temp_path: to facilitate rollback, the caller needs to supply
  // a temporary directory to save the original files if they exist under
  // dest_path.
  CopyTreeWorkItem(const std::wstring& source_path,
                   const std::wstring& dest_path,
                   const std::wstring& temp_dir,
                   CopyOverWriteOption overwrite_option,
                   const std::wstring& alternative_path);

  // Checks if the path specified is in use (and hence can not be deleted)
  bool IsFileInUse(const std::wstring& path);

  // Get a backup path that can keep the original files under dest_path_,
  // and set backup_path_ with the result.
  bool GetBackupPath();

  // Source path to copy files from.
  std::wstring source_path_;

  // Destination path to copy files to.
  std::wstring dest_path_;

  // Temporary directory that can be used.
  std::wstring temp_dir_;

  // Controls the behavior for overwriting.
  CopyOverWriteOption overwrite_option_;

  // If overwrite_option_ = NEW_NAME_IF_IN_USE, this variables stores the path
  // to be used if the file is in use and hence we want to copy it to a
  // different path.
  std::wstring alternative_path_;

  // Whether the source was copied to dest_path_
  bool copied_to_dest_path_;

  // Whether the original files have been moved to backup path under
  // temporary directory. If true, moving back is needed during rollback.
  bool moved_to_backup_;

  // Whether the source was copied to alternative_path_ because dest_path_
  // existed and was in use. Needed during rollback.
  bool copied_to_alternate_path_;

  // The full path in temporary directory that the original dest_path_ has
  // been moved to.
  std::wstring backup_path_;
};

#endif  // CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
