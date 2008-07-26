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

#include "base/registry.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/logging_installer.h"

SetRegValueWorkItem::~SetRegValueWorkItem() {
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                     std::wstring key_path,
                                     std::wstring value_name,
                                     std::wstring value_data,
                                     bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      value_data_str_(value_data),
      overwrite_(overwrite),
      status_(SET_VALUE),
      is_str_type_(true) {
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                     std::wstring key_path,
                                     std::wstring value_name,
                                     DWORD value_data,
                                     bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      value_data_dword_(value_data),
      overwrite_(overwrite),
      status_(SET_VALUE),
      is_str_type_(false) {
}

bool SetRegValueWorkItem::Do() {
  if (status_ != SET_VALUE) {
    // we already did something.
    LOG(ERROR) << "multiple calls to Do()";
    return false;
  }

  RegKey key;
  if (!key.Open(predefined_root_, key_path_.c_str(),
                KEY_READ | KEY_SET_VALUE)) {
    LOG(ERROR) << "can not open " << key_path_;
    status_ = VALUE_UNCHANGED;
    return false;
  }

  bool result = false;
  if (key.ValueExists(value_name_.c_str())) {
    if (overwrite_) {
      bool success = true;
      // Read previous value for rollback and write new value
      if (is_str_type_) {
        std::wstring data;
        if (key.ReadValue(value_name_.c_str(), &data)) {
          previous_value_str_.assign(data);
        }
        success = key.WriteValue(value_name_.c_str(), value_data_str_.c_str());
      } else {
        DWORD data;
        if (key.ReadValueDW(value_name_.c_str(), &data)) {
          previous_value_dword_ = data;
        }
        success = key.WriteValue(value_name_.c_str(), value_data_dword_);
      }
      if (success) {
        LOG(INFO) << "overwritten value for " << value_name_;
        status_ = VALUE_OVERWRITTEN;
        result = true;
      } else {
        LOG(ERROR) << "failed to overwrite value for " << value_name_;
        status_ = VALUE_UNCHANGED;
        result = false;
      }
    } else {
      LOG(INFO) << value_name_ << " exists. not changed ";
      status_ = VALUE_UNCHANGED;
      result = true;
    }
  } else {
    bool success = true;
    if (is_str_type_) {
      success = key.WriteValue(value_name_.c_str(), value_data_str_.c_str());
    } else {
      success = key.WriteValue(value_name_.c_str(), value_data_dword_);
    }
    if (success) {
      LOG(INFO) << "created value for " << value_name_;
      status_ = NEW_VALUE_CREATED;
      result = true;
    } else {
      LOG(ERROR) << "failed to create value for " << value_name_;
      status_ = VALUE_UNCHANGED;
      result = false;
    }
  }

  key.Close();
  return result;
}

void SetRegValueWorkItem::Rollback() {
  if (status_ == SET_VALUE || status_ == VALUE_ROLL_BACK)
    return;

  if (status_ == VALUE_UNCHANGED) {
    status_ = VALUE_ROLL_BACK;
    LOG(INFO) << "rollback: setting unchanged, nothing to do";
    return;
  }

  RegKey key;
  if (!key.Open(predefined_root_, key_path_.c_str(),
                KEY_READ | KEY_SET_VALUE)) {
    status_ = VALUE_ROLL_BACK;
    LOG(INFO) << "rollback: can not open " << key_path_;
    return;
  }

  std::wstring result_str(L" failed");
  if (status_ == NEW_VALUE_CREATED) {
    if (key.DeleteValue(value_name_.c_str()))
      result_str.assign(L" succeeded");
    LOG(INFO) << "rollback: deleting " << value_name_ << result_str;
  } else if (status_ == VALUE_OVERWRITTEN) {
    // try restore the previous value
    bool success = true;
    if (is_str_type_) {
      success = key.WriteValue(value_name_.c_str(),
                               previous_value_str_.c_str());
    } else {
      success = key.WriteValue(value_name_.c_str(), previous_value_dword_);
    }
    if (success)
      result_str.assign(L" succeeded");
    LOG(INFO) << "rollback: restoring " << value_name_ << result_str;
  } else {
    // Not reached.
  }

  status_ = VALUE_ROLL_BACK;
  key.Close();
  return;
}
