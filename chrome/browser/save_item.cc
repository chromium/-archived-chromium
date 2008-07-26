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

#include "chrome/browser/save_item.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/browser/save_file.h"
#include "chrome/browser/save_file_manager.h"
#include "chrome/browser/save_package.h"

// Constructor for SaveItem when creating each saving job.
SaveItem::SaveItem(const std::wstring& url,
                   const std::wstring& referrer,
                   SavePackage* package,
                   SaveFileCreateInfo::SaveFileSource save_source)
  : save_id_(-1),
    save_source_(save_source),
    url_(url),
    referrer_(referrer),
    total_bytes_(0),
    received_bytes_(0),
    state_(WAIT_START),
    package_(package),
    has_final_name_(false),
    is_success_(false) {
  DCHECK(package);
}

SaveItem::~SaveItem() {
}

// Set start state for save item.
void SaveItem::Start() {
  DCHECK(state_ == WAIT_START);
  state_ = IN_PROGRESS;
}

// If we've received more data than we were expecting (bad server info?),
// revert to 'unknown size mode'.
void SaveItem::UpdateSize(int64 bytes_so_far) {
  received_bytes_ = bytes_so_far;
  if (received_bytes_ >= total_bytes_)
    total_bytes_ = 0;
}

// Updates from the file thread may have been posted while this saving job
// was being canceled in the UI thread, so we'll accept them unless we're
// complete.
void SaveItem::Update(int64 bytes_so_far) {
  if (state_ != IN_PROGRESS) {
    NOTREACHED();
    return;
  }
  UpdateSize(bytes_so_far);
}

// Cancel this saving item job. If the job is not in progress, ignore
// this command. The SavePackage will each in-progress SaveItem's cancel
// when canceling whole saving page job.
void SaveItem::Cancel() {
  // If item is in WAIT_START mode, which means no request has been sent.
  // So we need not to cancel it.
  if (state_ != IN_PROGRESS) {
    // Small downloads might be complete before method has a chance to run.
    return;
  }
  state_ = CANCELED;
  is_success_ = false;
  Finish(received_bytes_, false);
  package_->SaveCanceled(this);
}

// Set finish state for a save item
void SaveItem::Finish(int64 size, bool is_success) {
  // When this function is called, the SaveItem should be one of following
  // three situations.
  // a) The data of this SaveItem is finished saving. So it should have
  // generated final name.
  // b) Error happened before the start of saving process. So no |save_id_| is
  // generated for this SaveItem and the |is_success_| should be false.
  // c) Error happened in the start of saving process, the SaveItem has a save
  // id, |is_success_| should be false, and the |size| should be 0.
  DCHECK(has_final_name() || (save_id_ == -1 && !is_success_) ||
         (save_id_ != -1 && !is_success_ && !size));
  state_ = COMPLETE;
  is_success_ = is_success;
  UpdateSize(size);
}

// Calculate the percentage of the save item
int SaveItem::PercentComplete() const {
  switch (state_) {
    case COMPLETE:
    case CANCELED:
      return 100;
    case WAIT_START:
      return 0;
    case IN_PROGRESS: {
      int percent = 0;
      if (total_bytes_ > 0)
        percent = static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
      return percent;
    }
    default: {
      NOTREACHED();
      return -1;
    }
  }
}

// Rename the save item with new path.
void SaveItem::Rename(const std::wstring& full_path) {
  DCHECK(!full_path.empty() && !has_final_name());
  full_path_ = full_path;
  file_name_ = file_util::GetFilenameFromPath(full_path_);
  has_final_name_ = true;
}

void SaveItem::SetSaveId(int32 save_id) {
  DCHECK(save_id_ == -1);
  save_id_ = save_id;
}

void SaveItem::SetTotalBytes(int64 total_bytes) {
  DCHECK(total_bytes_ == 0);
  total_bytes_ = total_bytes;
}
