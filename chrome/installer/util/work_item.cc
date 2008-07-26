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

#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item_list.h"

WorkItem::WorkItem() {
}

WorkItem::~WorkItem() {
}

CopyTreeWorkItem* WorkItem::CreateCopyTreeWorkItem(
    std::wstring source_path, std::wstring dest_path, std::wstring temp_dir,
    CopyOverWriteOption overwrite_option, std::wstring alternative_path) {
  return new CopyTreeWorkItem(source_path, dest_path, temp_dir,
                              overwrite_option, alternative_path);
}

CreateDirWorkItem* WorkItem::CreateCreateDirWorkItem(std::wstring path) {
  return new CreateDirWorkItem(path);
}

CreateRegKeyWorkItem* WorkItem::CreateCreateRegKeyWorkItem(
    HKEY predefined_root, std::wstring path) {
  return new CreateRegKeyWorkItem(predefined_root, path);
}

DeleteTreeWorkItem* WorkItem::CreateDeleteTreeWorkItem(std::wstring root_path,
                                                       std::wstring key_path) {
  return new DeleteTreeWorkItem(root_path, key_path);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root, std::wstring key_path,
    std::wstring value_name, std::wstring value_data, bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path,
                               value_name, value_data, overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root, std::wstring key_path,
    std::wstring value_name, DWORD value_data, bool overwrite) {
  return new SetRegValueWorkItem(predefined_root, key_path,
                               value_name, value_data, overwrite);
}

WorkItemList* WorkItem::CreateWorkItemList() {
  return new WorkItemList();
}

std::wstring WorkItem::Dump() {
  return std::wstring(L"Work Item");
}
