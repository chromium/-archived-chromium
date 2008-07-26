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

#ifndef CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__

#include <string>
#include <windows.h>
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that sets a registry value with REG_SZ or REG_DWORD
// type at the specified path. The value is only set if the target key exists.
class SetRegValueWorkItem : public WorkItem {
 public:
  virtual ~SetRegValueWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  enum SettingStatus {
    // The status before Do is called.
    SET_VALUE,
    // One possible outcome after Do(). A new value is created under the key.
    NEW_VALUE_CREATED,
    // One possible outcome after Do(). The previous value under the key has
    // been overwritten.
    VALUE_OVERWRITTEN,
    // One possible outcome after Do(). No change is applied, either
    // because we are not allowed to overwrite the previous value, or due to
    // some errors like the key does not exist.
    VALUE_UNCHANGED,
    // The status after Do and Rollback is called.
    VALUE_ROLL_BACK
  };

  SetRegValueWorkItem(HKEY predefined_root, std::wstring key_path,
                      std::wstring value_name, std::wstring value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root, std::wstring key_path,
                      std::wstring value_name, DWORD value_data,
                      bool overwrite);

  // Root key of the target key under which the value is set. The root key can
  // only be one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the target key under which the value is set.
  std::wstring key_path_;

  // Name of the value to be set.
  std::wstring value_name_;

  // Data of the value to be set.
  std::wstring value_data_str_;  // if data is of type REG_SZ
  DWORD value_data_dword_;  // if data is of type REG_DWORD

  // Whether to overwrite the existing value under the target key.
  bool overwrite_;

  // boolean that tells whether data value is of type REG_SZ.
  bool is_str_type_;

  SettingStatus status_;

  // Data of the previous value.
  std::wstring previous_value_str_;  // if data is of type REG_SZ
  DWORD previous_value_dword_;  // if data is of type REG_DWORD
};

#endif  // CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__
