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

#ifndef CHROME_BROWSER_SAVE_FILE_H__
#define CHROME_BROWSER_SAVE_FILE_H__

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/save_types.h"

// SaveFile ----------------------------------------------------------------

// These objects live exclusively on the file thread and handle the writing
// operations for one save item. These objects live only for the duration that
// the saving job is 'in progress': once the saving job has been completed or
// canceled, the SaveFile is destroyed. One SaveFile object represents one item
// in a save session.
class SaveFile {
 public:
  explicit SaveFile(const SaveFileCreateInfo* info);
  ~SaveFile();

  // Write a new chunk of data to the file. Returns true on success.
  bool AppendDataToFile(const char* data, int data_len);

  // Abort the saving job and automatically close the file.
  void Cancel();

  // Rename the saved file. Returns 'true' if the rename was successful.
  bool Rename(const std::wstring& full_path);

  void Finish();

  // Accessors.
  int save_id() const { return info_->save_id; }
  int render_process_id() const { return info_->render_process_id; }
  int render_view_id() const { return info_->render_view_id; }
  int request_id() const { return info_->request_id; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return info_->save_source;
  }

  int64 bytes_so_far() const { return bytes_so_far_; }
  std::wstring full_path() const { return full_path_; }
  bool path_renamed() const { return path_renamed_; }
  bool in_progress() const { return in_progress_; }

 private:
  // Open or Close the OS file handle. The file is opened in the constructor
  // based on creation information passed to it, and automatically closed in
  // the destructor.
  void Close();
  bool Open(const wchar_t* open_mode);

  scoped_ptr<const SaveFileCreateInfo> info_;

  // OS file handle for writing
  FILE* file_;

  // Amount of data received up to this point. We may not know in advance how
  // much data to expect since some servers don't provide that information.
  int64 bytes_so_far_;

  // Full path to the saved file including the file name.
  std::wstring full_path_;

  // Whether the saved file is still using its initial temporary path.
  bool path_renamed_;

  // Whether the saved file is still receiving data.
  bool in_progress_;

  DISALLOW_EVIL_CONSTRUCTORS(SaveFile);
};

#endif  // CHROME_BROWSER_SAVE_FILE_H__
