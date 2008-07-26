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

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H__
#define CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H__

#include <list>
#include <string>
#include <windows.h>
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively contains a list of WorkItems. Thus it
// provides functionalities to carry out or roll back the sequence of actions
// defined by the list of WorkItems it contains.
// The WorkItems are executed in the same order as they are added to the list.
class WorkItemList : public WorkItem {
 public:
  virtual ~WorkItemList();

  // Execute the WorkItems in the same order as they are added to the list.
  // It aborts as soon as one WorkItem fails.
  virtual bool Do();

  // Rollback the WorkItems in the reverse order as they are executed.
  virtual void Rollback();

  // Add a WorkItem to the list. Return true if the WorkItem is successfully
  // added. Return false otherwise.
  // A WorkItem can only be added to the list before the list's DO() is called.
  // Once a WorkItem is added to the list. The list owns the WorkItem.
  bool AddWorkItem(WorkItem* work_item);

  // Add a CopyTreeWorkItem to the list of work items.
  bool AddCopyTreeWorkItem(std::wstring source_path, std::wstring dest_path,
                           std::wstring temp_dir,
                           CopyOverWriteOption overwrite_option,
                           std::wstring alternative_path = L"");

  // Add a CreateDirWorkItem that creates a directory at the given path.
  bool AddCreateDirWorkItem(std::wstring path);

  // Add a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  bool AddCreateRegKeyWorkItem(HKEY predefined_root, std::wstring path);

  // Add a DeleteTreeWorkItem that recursively deletes a file system
  // hierarchy at the given root path. A key file can be optionally specified
  // by key_path.
  bool AddDeleteTreeWorkItem(std::wstring root_path, std::wstring key_path);

  // Add a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  bool AddSetRegValueWorkItem(HKEY predefined_root, std::wstring key_path,
                              std::wstring value_name, std::wstring value_data,
                              bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  bool AddSetRegValueWorkItem(HKEY predefined_root, std::wstring key_path,
                              std::wstring value_name, DWORD value_data,
                              bool overwrite);

 private:
  friend class WorkItem;

  typedef std::list<WorkItem*> WorkItems;
  typedef WorkItems::iterator WorkItemIterator;

  enum ListStatus {
    // List has not been executed. Ok to add new WorkItem.
    ADD_ITEM,
    // List has been executed. Can not add new WorkItem.
    LIST_EXECUTED,
    // List has been executed and rolled back. No further action is acceptable.
    LIST_ROLLED_BACK
  };

  WorkItemList();

  ListStatus status_;

  // The list of WorkItems, in the order of them being added.
  WorkItems list_;

  // The list of executed WorkItems, in the reverse order of them being
  // executed.
  WorkItems executed_list_;
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H__
