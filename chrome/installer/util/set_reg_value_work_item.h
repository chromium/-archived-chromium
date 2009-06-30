// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      const std::wstring& value_name,
                      const std::wstring& value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root, const std::wstring& key_path,
                      const std::wstring& value_name, DWORD value_data,
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
