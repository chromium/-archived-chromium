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

#include "chrome/browser/save_page_model.h"

#include "base/string_util.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/save_package.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

SavePageModel::SavePageModel(SavePackage* save, DownloadItem* download)
    : save_(save),
      download_(download) {
}

void SavePageModel::CancelTask() {
  save_->Cancel(true);
}

std::wstring SavePageModel::GetStatusText() {
  int64 size = download_->received_bytes();
  int64 total_size = download_->total_bytes();

  std::wstring status_text;
  switch (download_->state()) {
    case DownloadItem::IN_PROGRESS:
      status_text = l10n_util::GetStringF(IDS_SAVE_PAGE_PROGRESS,
                                          FormatNumber(size),
                                          FormatNumber(total_size));
      break;
    case DownloadItem::COMPLETE:
      status_text = l10n_util::GetString(IDS_SAVE_PAGE_STATUS_COMPLETED);
      break;
    case DownloadItem::CANCELLED:
      status_text = l10n_util::GetString(IDS_SAVE_PAGE_STATUS_CANCELLED);
      break;
    case DownloadItem::REMOVING:
      break;
    default:
      NOTREACHED();
  }

  return status_text;
}
