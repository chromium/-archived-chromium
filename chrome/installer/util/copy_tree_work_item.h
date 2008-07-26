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

#ifndef CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H__

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
  CopyTreeWorkItem(std::wstring source_path, std::wstring dest_path,
                   std::wstring temp_dir, CopyOverWriteOption overwrite_option,
                   std::wstring alternative_path);

  // Checks if the path specified is in use (and hence can not be deleted)
  bool IsFileInUse(std::wstring path);

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

  // If overwrite_option_ = RENAME_IF_IN_USE, this variables stores the path
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

#endif  // CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H__
