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

#ifndef CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H__

#include <string>
#include <windows.h>
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively deletes a file system hierarchy at the
// given root path. The file system hierarchy could be a single file, or a
// directory.
// The file system hierarchy to be deleted can have a key file. If the key file
// is specified, deletion will be performed only if the key file is not in use.
class DeleteTreeWorkItem : public WorkItem {
 public:
  virtual ~DeleteTreeWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  // Get a backup path that can keep root_path_ or key_path_
  bool GetBackupPath(std::wstring for_path, std::wstring* backup_path);

  DeleteTreeWorkItem(std::wstring root_path, std::wstring key_path);

  // Root path to delete.
  std::wstring root_path_;

  // Path to the key file. If the key file is specified, deletion will be
  // performed only if the key file is not in use.
  std::wstring key_path_;

  // The full path in temporary directory that the original root_path_ has
  // been moved to.
  std::wstring backup_path_;

  // The full path in temporary directory that the original key_path_ has
  // been moved to.
  std::wstring key_backup_path_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H__
