// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_page_model.h"

#include "base/string_util.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

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

