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

#include "chrome/browser/download_item_model.h"

#include "base/string_util.h"
#include "chrome/browser/download_manager.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"

#include "generated_resources.h"

DownloadItemModel::DownloadItemModel(DownloadItem* download)
    : download_(download) {
}

void DownloadItemModel::CancelTask() {
  download_->Cancel(true /* update history service */);
}

std::wstring DownloadItemModel::GetStatusText() {
  int64 size = download_->received_bytes();
  int64 total = download_->total_bytes();

  DataUnits amount_units = GetByteDisplayUnits(total);
  const std::wstring simple_size = FormatBytes(size, amount_units, false);

  // In RTL locales, we render the text "size/total" in an RTL context. This
  // is problematic since a string such as "123/456 MB" is displayed
  // as "MB 123/456" because it ends with an LTR run. In order to solve this,
  // we mark the total string as an LTR string if the UI layout is
  // right-to-left so that the string "456 MB" is treated as an LTR run.
  std::wstring simple_total = FormatBytes(total, amount_units, true);
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&simple_total);

  TimeDelta remaining;
  std::wstring simple_time;
  if (download_->state() == DownloadItem::IN_PROGRESS &&
      download_->is_paused()) {
    simple_time = l10n_util::GetString(IDS_DOWNLOAD_PROGRESS_PAUSED);
  } else if (download_->TimeRemaining(&remaining)) {
    simple_time = download_->open_when_complete() ?
                      TimeFormat::TimeRemainingShort(remaining) :
                      TimeFormat::TimeRemaining(remaining);
  }

  std::wstring status_text;
  switch (download_->state()) {
    case DownloadItem::IN_PROGRESS:
      if (download_->open_when_complete()) {
        if (simple_time.empty()) {
          status_text =
              l10n_util::GetString(IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE);
        } else {
          status_text = l10n_util::GetStringF(IDS_DOWNLOAD_STATUS_OPEN_IN,
                                              simple_time);
        }
      } else {
        if (simple_time.empty()) {
          // Instead of displaying "0 B" we keep the "Starting..." string.
          status_text = (size == 0) ?
              l10n_util::GetString(IDS_DOWNLOAD_STATUS_STARTING) :
              FormatBytes(size, GetByteDisplayUnits(size), true);
        } else {
          status_text = l10n_util::GetStringF(IDS_DOWNLOAD_STATUS_IN_PROGRESS,
                                              simple_size,
                                              simple_total,
                                              simple_time);
        }
      }
      break;
    case DownloadItem::COMPLETE:
      status_text.clear();
      break;
    case DownloadItem::CANCELLED:
      status_text = l10n_util::GetStringF(IDS_DOWNLOAD_STATUS_CANCELLED,
                                          simple_size);
      break;
    case DownloadItem::REMOVING:
      break;
    default:
      NOTREACHED();
  }

  return status_text;
}
