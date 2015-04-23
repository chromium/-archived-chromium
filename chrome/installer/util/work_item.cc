// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/work_item.h"

#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/delete_reg_value_work_item.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "chrome/installer/util/self_reg_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item_list.h"

WorkItem::WorkItem() {
}

WorkItem::~WorkItem() {
}

CopyTreeWorkItem* WorkItem::CreateCopyTreeWorkItem(
    const std::wstring& source_path,
    const std::wstring& dest_path,
    const std::wstring& temp_dir,
    CopyOverWriteOption overwrite_option,
    const std::wstring& alternative_path) {
  return new CopyTreeWorkItem(source_path, dest_path, temp_dir,
                              overwrite_option, alternative_path);
}

CreateDirWorkItem* WorkItem::CreateCreateDirWorkItem(const std::wstring& path) {
  return new CreateDirWorkItem(path);
}

CreateRegKeyWorkItem* WorkItem::CreateCreateRegKeyWorkItem(
    HKEY predefined_root, const std::wstring& path) {
  return new CreateRegKeyWorkItem(predefined_root, path);
}

DeleteRegValueWorkItem* WorkItem::CreateDeleteRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    const std::wstring& value_name,
    bool is_str_type) {
  return new DeleteRegValueWorkItem(predefined_root, key_path,
                                    value_name, is_str_type);
}

DeleteTreeWorkItem* WorkItem::CreateDeleteTreeWorkItem(
    const std::wstring& root_path, const std::wstring& key_path) {
  return new DeleteTreeWorkItem(root_path, key_path);
}

MoveTreeWorkItem* WorkItem::CreateMoveTreeWorkItem(
    const std::wstring& source_path,
    const std::wstring& dest_path,
    const std::wstring& temp_dir) {
  return new MoveTreeWorkItem(source_path, dest_path, temp_dir);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    const std::wstring& value_name,
    const std::wstring& value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path,
                                 value_name, value_data, overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    const std::wstring& value_name,
    DWORD value_data, bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path,
                                 value_name, value_data, overwrite);
}

SelfRegWorkItem* WorkItem::CreateSelfRegWorkItem(const std::wstring& dll_path,
                                                 bool do_register) {
  return new SelfRegWorkItem(dll_path, do_register);
}

WorkItemList* WorkItem::CreateWorkItemList() {
  return new WorkItemList();
}

std::wstring WorkItem::Dump() {
  return std::wstring(L"Work Item");
}
