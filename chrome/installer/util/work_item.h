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
//
// Base class for managing an action of a sequence of actions to be carried
// out during install/update/uninstall. Supports rollback of actions if this
// process fails.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_WORK_ITEM_H__

#include <string>
#include <windows.h>

class CopyTreeWorkItem;
class CreateDirWorkItem;
class CreateRegKeyWorkItem;
class DeleteTreeWorkItem;
class SetRegValueWorkItem;
class WorkItemList;

// A base class that defines APIs to perform/rollback an action or a
// sequence of actions during install/update/uninstall.
class WorkItem {
 public:
  // Possible states
  typedef enum CopyOverWriteOption {
    ALWAYS,  // Always overwrite regardless of what existed before.
    NEVER,  // Not used currently.
    IF_DIFFERENT,  // Overwrite if different. Currently only applies to file.
    RENAME_IF_IN_USE  // Copy to a new path instead of overwriting (only files).
  };

  virtual ~WorkItem();

  // Create a CopyTreeWorkItem that recursively copies a file system hierarchy
  // from source path to destination path. If overwrite_option is ALWAYS, the
  // created CopyTreeWorkItem always overwrites files. If overwrite_option is
  // RENAME_IF_IN_USE, file is copied with an alternate name specified by
  // alternative_path.
  static CopyTreeWorkItem* CreateCopyTreeWorkItem(std::wstring source_path,
      std::wstring dest_path, std::wstring temp_dir,
      CopyOverWriteOption overwrite_option,
      std::wstring alternative_path = L"");

  // Create a CreateDirWorkItem that creates a directory at the given path.
  static CreateDirWorkItem* CreateCreateDirWorkItem(std::wstring path);

  // Create a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  static CreateRegKeyWorkItem* CreateCreateRegKeyWorkItem(
      HKEY predefined_root, std::wstring path);

  // Create a DeleteTreeWorkItem that recursively deletes a file system
  // hierarchy at the given root path. A key file can be optionally specified
  // by key_path.
  static DeleteTreeWorkItem* CreateDeleteTreeWorkItem(std::wstring root_path,
                                                      std::wstring key_path);

  // Create a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root, std::wstring key_path,
      std::wstring value_name, std::wstring value_data, bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root, std::wstring key_path,
      std::wstring value_name, DWORD value_data, bool overwrite);

  // Create an empty WorkItemList. A WorkItemList can recursively contains
  // a list of WorkItems.
  static WorkItemList* CreateWorkItemList();

  // Perform the actions of WorkItem. Returns true if success, returns false
  // otherwise.
  // If the WorkItem is transactional, then Do() is done as a transaction.
  // If it returns false, there will be no change on the system.
  virtual bool Do() = 0;

  // Rollback any actions previously carried out by this WorkItem. If the
  // WorkItem is transactional, then the previous actions can be fully
  // rolled back. If the WorkItem is non-transactional, the rollback is a
  // best effort.
  virtual void Rollback() = 0;

  // Return true if the WorkItem is transactional, return false if
  // non-transactional.
  virtual bool IsTransactional() { return false; }

  // For diagnostics.
  virtual std::wstring Dump();

 protected:
  WorkItem();
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_H__
