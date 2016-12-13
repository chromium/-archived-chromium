// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_value_work_item.h"

#include "base/registry.h"
#include "chrome/installer/util/logging_installer.h"

DeleteRegValueWorkItem::DeleteRegValueWorkItem(HKEY predefined_root,
                                               const std::wstring& key_path,
                                               const std::wstring& value_name,
                                               bool is_str_type)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      is_str_type_(is_str_type),
      status_(DELETE_VALUE),
      old_dw_(0) {
}

DeleteRegValueWorkItem::~DeleteRegValueWorkItem() {
}

bool DeleteRegValueWorkItem::Do() {
  if (status_ != DELETE_VALUE) {
    // we already did something.
    LOG(ERROR) << "multiple calls to Do()";
    return false;
  }

  RegKey key;
  status_ = VALUE_UNCHANGED;
  bool result = false;
  if (!key.Open(predefined_root_, key_path_.c_str(), KEY_READ | KEY_WRITE)) {
    LOG(ERROR) << "can not open " << key_path_;
  } else if (!key.ValueExists(value_name_.c_str())) {
    status_ = VALUE_NOT_FOUND;
    result = true;
  // Read previous value for rollback and delete
  } else if (((is_str_type_ && key.ReadValue(value_name_.c_str(),
                                             &old_str_)) ||
              (!is_str_type_ && key.ReadValueDW(value_name_.c_str(),
                                                &old_dw_))) &&
             (key.DeleteValue(value_name_.c_str()))) {
    status_ = VALUE_DELETED;
    result = true;
  } else {
    LOG(ERROR) << "failed to read/delete value " << value_name_;
  }

  key.Close();
  return result;
}

void DeleteRegValueWorkItem::Rollback() {
  if (status_ == DELETE_VALUE || status_ == VALUE_ROLLED_BACK) {
    return;
  } else if (status_ == VALUE_UNCHANGED || status_ == VALUE_NOT_FOUND) {
    status_ = VALUE_ROLLED_BACK;
    LOG(INFO) << "rollback: setting unchanged, nothing to do";
    return;
  }

  // At this point only possible state is VALUE_DELETED.
  RegKey key;
  if (!key.Open(predefined_root_, key_path_.c_str(), KEY_READ | KEY_WRITE)) {
    LOG(ERROR) << "rollback: can not open " << key_path_;
  // try to restore the previous value
  } else if ((is_str_type_ && key.WriteValue(value_name_.c_str(),
                                             old_str_.c_str())) ||
             (!is_str_type_ && key.WriteValue(value_name_.c_str(),
                                              old_dw_))) {
    status_ = VALUE_ROLLED_BACK;
    LOG(INFO) << "rollback: restored " << value_name_;
  } else {
    LOG(ERROR) << "failed to restore value " << value_name_;
  }

  key.Close();
  return;
}
