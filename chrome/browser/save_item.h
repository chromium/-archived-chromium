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
//
#ifndef CHROME_BROWSER_SAVE_ITEM_H__
#define CHROME_BROWSER_SAVE_ITEM_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/save_types.h"

class SavePackage;

// One SaveItem per save file. This is the model class that stores all the
// state for one save file.
class SaveItem {
 public:
  enum SaveState {
    WAIT_START,
    IN_PROGRESS,
    COMPLETE,
    CANCELED
  };

  SaveItem(const std::wstring& url,
           const std::wstring& referrer,
           SavePackage* package,
           SaveFileCreateInfo::SaveFileSource save_source);

  ~SaveItem();

  void Start();

  // Received a new chunk of data.
  void Update(int64 bytes_so_far);

  // Cancel saving item.
  void Cancel();

  // Saving operation completed.
  void Finish(int64 size, bool is_success);

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete() const;

  // Update path for SaveItem, the actual file is renamed on the file thread.
  void Rename(const std::wstring& full_path);

  void SetSaveId(int32 save_id);

  void SetTotalBytes(int64 total_bytes);

  // Accessors.
  SaveState state() const { return state_; }
  const std::wstring full_path() const { return full_path_; }
  const std::wstring file_name() const { return file_name_; }
  const std::wstring& url() const { return url_; }
  const std::wstring& referrer() const { return referrer_; }
  int64 total_bytes() const { return total_bytes_; }
  int64 received_bytes() const { return received_bytes_; }
  int32 save_id() const { return save_id_; }
  bool has_final_name() const { return has_final_name_; }
  bool success() const { return is_success_; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return save_source_;
  }
  SavePackage* package() const { return package_; }

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Request ID assigned by the ResourceDispatcherHost.
  int32 save_id_;

  // Full path to the save item file.
  std::wstring full_path_;

  // Short display version of the file.
  std::wstring file_name_;

  // The URL for this save item.
  std::wstring url_;
  std::wstring referrer_;

  // Total bytes expected.
  int64 total_bytes_;

  // Current received bytes.
  int64 received_bytes_;

  // The current state of this save item.
  SaveState state_;

  // Specifies if this name is a final or not.
  bool has_final_name_;

  // Flag indicates whether SaveItem has error while in saving process.
  bool is_success_;

  SaveFileCreateInfo::SaveFileSource save_source_;

  // Our owning object.
  SavePackage* package_;

  DISALLOW_EVIL_CONSTRUCTORS(SaveItem);
};

#endif  // CHROME_BROWSER_SAVE_ITEM_H__
