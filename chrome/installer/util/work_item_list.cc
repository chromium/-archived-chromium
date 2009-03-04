// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

bool WorkItemList::AddDeleteRegValueWorkItem(HKEY predefined_root,
                                             std::wstring key_path,
                                             std::wstring value_name,
                                             bool is_str_type) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateDeleteRegValueWorkItem(predefined_root, key_path,
                                             value_name, is_str_type));
  return AddWorkItem(item);
}

bool WorkItemList::AddDeleteTreeWorkItem(std::wstring root_path,
                                         std::wstring key_path) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateDeleteTreeWorkItem(root_path, key_path));
  return AddWorkItem(item);
}

bool WorkItemList::AddMoveTreeWorkItem(std::wstring source_path,
                                       std::wstring dest_path,
                                       std::wstring temp_dir) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateMoveTreeWorkItem(source_path, dest_path, temp_dir));
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

