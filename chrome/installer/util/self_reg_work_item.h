// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SELF_REG_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_SELF_REG_WORK_ITEM_H__

#include <windows.h>
#include <string>

#include "chrome/installer/util/work_item.h"

// Registers or unregisters the DLL at the given path.
class SelfRegWorkItem : public WorkItem {
 public:
  virtual ~SelfRegWorkItem();

  virtual bool Do();
  virtual void Rollback();

 private:
  friend class WorkItem;

  SelfRegWorkItem(const std::wstring& dll_path, bool do_register);

  // Examines the DLL at dll_path looking for either DllRegisterServer (if
  // do_register is true) or DllUnregisterServer (if do_register is false).
  // Returns true if the DLL exports the function and it a call to it
  // succeeds, false otherwise.
  bool RegisterDll(bool do_register);

  // The path to the dll to be registered.
  std::wstring dll_path_;

  // Whether this work item will register or unregister the dll. The rollback
  // action just inverts this parameter.
  bool do_register_;
};

#endif  // CHROME_INSTALLER_UTIL_SELF_REG_WORK_ITEM_H__
