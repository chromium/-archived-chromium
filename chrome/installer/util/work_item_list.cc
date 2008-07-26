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

#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/work_item_list.h"

WorkItemList::~WorkItemList() {
  for (WorkItemIterator itr = list_.begin(); itr != list_.end(); ++itr) {
    delete (*itr);
  }
  for (WorkItemIterator itr = executed_list_.begin();
       itr != executed_list_.end(); ++itr) {
    delete (*itr);
  }
}

WorkItemList::WorkItemList()
    : status_(ADD_ITEM) {
}

bool WorkItemList::Do() {
  if (status_ != ADD_ITEM)
    return false;

  bool result = true;
  while (!list_.empty()) {
    WorkItem* work_item = list_.front();
    list_.pop_front();
    executed_list_.push_front(work_item);
    if (!work_item->Do()) {
      LOG(ERROR) << "list execution failed";
      result = false;
      break;
    }
  }

  if (result)
    LOG(INFO) << "list execution succeeded";

  status_ = LIST_EXECUTED;
  return result;
}

void WorkItemList::Rollback() {
  if (status_ != LIST_EXECUTED)
    return;

  for (WorkItemIterator itr = executed_list_.begin();
       itr != executed_list_.end(); ++itr) {
    (*itr)->Rollback();
  }

  status_ = LIST_ROLLED_BACK;
  return;
}

bool WorkItemList::AddWorkItem(WorkItem* work_item) {
  if (status_ != ADD_ITEM)
    return false;

  list_.push_back(work_item);
  return true;
}

bool WorkItemList::AddCopyTreeWorkItem(std::wstring source_path,
                                       std::wstring dest_path,
                                       std::wstring temp_dir,
                                       CopyOverWriteOption overwrite_option,
                                       std::wstring alternative_path) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateCopyTreeWorkItem(source_path, dest_path, temp_dir,
                                       overwrite_option, alternative_path));
  return AddWorkItem(item);
}

bool WorkItemList::AddCreateDirWorkItem(std::wstring path) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(path));
  return AddWorkItem(item);
}

bool WorkItemList::AddCreateRegKeyWorkItem(HKEY predefined_root,
                                           std::wstring path) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(predefined_root, path));
  return AddWorkItem(item);
}

bool WorkItemList::AddDeleteTreeWorkItem(std::wstring root_path,
                                         std::wstring key_path) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateDeleteTreeWorkItem(root_path, key_path));
  return AddWorkItem(item);
}

bool WorkItemList::AddSetRegValueWorkItem(HKEY predefined_root,
                                          std::wstring key_path,
                                          std::wstring value_name,
                                          std::wstring value_data,
                                          bool overwrite) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(predefined_root, key_path, value_name,
                                        value_data, overwrite));
  return AddWorkItem(item);
}

bool WorkItemList::AddSetRegValueWorkItem(HKEY predefined_root,
                                          std::wstring key_path,
                                          std::wstring value_name,
                                          DWORD value_data,
                                          bool overwrite) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(predefined_root, key_path, value_name,
                                        value_data, overwrite));
  return AddWorkItem(item);
}
