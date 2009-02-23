// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include "base/string_util.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"

using base::TimeDelta;

// -----------------------------------------------------------------------------
// DownloadItemModel

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

// -----------------------------------------------------------------------------
// SavePageModel

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

