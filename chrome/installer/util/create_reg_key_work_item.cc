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

#include "shlwapi.h"

#include "base/file_util.h"
#include "base/registry.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/logging_installer.h"

CreateRegKeyWorkItem::~CreateRegKeyWorkItem() {
}

CreateRegKeyWorkItem::CreateRegKeyWorkItem(HKEY predefined_root,
                                           std::wstring path)
    : predefined_root_(predefined_root),
      path_(path),
      key_created_(false) {
}

bool CreateRegKeyWorkItem::Do() {
  if(!InitKeyList()) {
    // Nothing needs to be done here.
    LOG(INFO) << "no key to create";
    return true;
  }

  RegKey key;
  std::wstring key_path;

  // To create keys, we iterate from back to front.
  for (size_t i = key_list_.size(); i > 0; i--) {
    DWORD disposition;
    key_path.assign(key_list_[i - 1]);

    if(key.CreateWithDisposition(predefined_root_, key_path.c_str(),
                                 &disposition)) {
      if (disposition == REG_OPENED_EXISTING_KEY) {
        if (key_created_) {
          // This should not happen. Someone created a subkey under the key
          // we just created?
          LOG(ERROR) << key_path << " exists, this is not expected.";
          return false;
        }
        LOG(INFO) << key_path << " exists";
        // Remove the key path from list if it is already present.
        key_list_.pop_back();
      } else if (disposition == REG_CREATED_NEW_KEY){
        LOG(INFO) << "created " << key_path;
        key_created_ = true;
      } else {
        LOG(ERROR) << "unkown disposition";
        return false;
      }
    } else {
      LOG(ERROR) << "Failed to create " << key_path;
      return false;
    }
  }

  return true;
}

void CreateRegKeyWorkItem::Rollback() {
  if (!key_created_)
    return;

  std::wstring key_path;
  // To delete keys, we iterate from front to back.
  std::vector<std::wstring>::iterator itr;
  for (itr = key_list_.begin(); itr != key_list_.end(); ++itr) {
    key_path.assign(*itr);
    if (SHDeleteEmptyKey(predefined_root_, key_path.c_str()) ==
        ERROR_SUCCESS) {
      LOG(INFO) << "rollback: delete " << key_path;
    } else {
      LOG(INFO) << "rollback: can not delete " << key_path;
      // The key might have been deleted, but we don't reliably know what
      // error code(s) are returned in this case. So we just keep tring delete
      // the rest.
    }
  }

  key_created_ = false;
  key_list_.clear();
  return;
}

bool CreateRegKeyWorkItem::InitKeyList() {
  if (path_.empty())
    return false;

  std::wstring key_path(path_);

  do {
    key_list_.push_back(key_path);
    // This is pure string operation so it does not matter whether the
    // path is file path or registry path.
    file_util::UpOneDirectoryOrEmpty(&key_path);
  } while(!key_path.empty());

  return true;
}
