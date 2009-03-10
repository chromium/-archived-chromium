// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H__

#include <string>
#include <vector>

#include <windows.h>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that creates a registry key at the given path.
// It also creates all necessary intermediate keys if they do not exist.
class CreateRegKeyWorkItem : public WorkItem {
 public:
  virtual ~CreateRegKeyWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  CreateRegKeyWorkItem(HKEY predefined_root, std::wstring path);

  // Initialize key_list_ by adding all paths of keys from predefined_root_
  // to path_. Returns true if key_list_ is non empty.
  bool InitKeyList();

  // Root key under which we create the new key. The root key can only be
  // one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the key to be created.
  std::wstring path_;

  // List of paths to all keys that need to be created from predefined_root_
  // to path_.
  std::vector<std::wstring> key_list_;

  // Whether any key has been created.
  bool key_created_;
};

#endif  // CHROME_INSTALLER_UTIL_CREATE_REG_KEY_WORK_ITEM_H__
