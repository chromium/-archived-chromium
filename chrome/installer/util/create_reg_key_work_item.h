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
