// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "base/file_util.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/download/download_util.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// DownloadShelf ---------------------------------------------------------------

void DownloadShelf::ShowAllDownloads() {
  Profile* profile = tab_contents_->profile();
  if (profile)
    UserMetrics::RecordAction(L"ShowDownloads", profile);
  GURL url = DownloadsUI::GetBaseURL();
  tab_contents_->OpenURL(url, GURL(), NEW_FOREGROUND_TAB,
      PageTransition::AUTO_BOOKMARK);
}

void DownloadShelf::ChangeTabContents(TabContents* old_contents,
                                      TabContents* new_contents) {
  DCHECK(old_contents == tab_contents_);
  tab_contents_ = new_contents;
}

// DownloadShelfContextMenu ----------------------------------------------------

DownloadShelfContextMenu::DownloadShelfContextMenu(
    BaseDownloadItemModel* download_model)
    : download_(download_model->download()),
      model_(download_model) {
}

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
}

bool DownloadShelfContextMenu::ItemIsChecked(int id) const {
  switch (id) {
    case OPEN_WHEN_COMPLETE:
      return download_->open_when_complete();
    case ALWAYS_OPEN_TYPE: {
      const FilePath::StringType extension =
          file_util::GetFileExtensionFromPath(download_->full_path());
      return download_->manager()->ShouldOpenFileExtension(extension);
    }
  }
  return false;
}

bool DownloadShelfContextMenu::ItemIsDefault(int id) const {
  return id == OPEN_WHEN_COMPLETE;
}

std::wstring DownloadShelfContextMenu::GetItemLabel(int id) const {
  switch (id) {
    case SHOW_IN_FOLDER:
      return l10n_util::GetString(IDS_DOWNLOAD_LINK_SHOW);
    case OPEN_WHEN_COMPLETE:
      if (download_->state() == DownloadItem::IN_PROGRESS)
        return l10n_util::GetString(IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_OPEN);
    case ALWAYS_OPEN_TYPE:
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE);
    case CANCEL:
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_CANCEL);
    default:
      NOTREACHED();
  }
  return std::wstring();
}

bool DownloadShelfContextMenu::IsItemCommandEnabled(int id) const {
  switch (id) {
    case SHOW_IN_FOLDER:
    case OPEN_WHEN_COMPLETE:
      return download_->state() != DownloadItem::CANCELLED;
    case ALWAYS_OPEN_TYPE:
#if defined(OS_WIN)
      return download_util::CanOpenDownload(download_);
#else
      // TODO(port): port download_util
      NOTIMPLEMENTED();
      return true;
#endif
    case CANCEL:
      return download_->state() == DownloadItem::IN_PROGRESS;
    default:
      return id > 0 && id < MENU_LAST;
  }
}

void DownloadShelfContextMenu::ExecuteItemCommand(int id) {
  switch (id) {
    case SHOW_IN_FOLDER:
      download_->manager()->ShowDownloadInShell(download_);
      break;
    case OPEN_WHEN_COMPLETE:
#if defined(OS_WIN)
      download_util::OpenDownload(download_);
#else
      // TODO(port): port download_util
      NOTIMPLEMENTED();
#endif
      break;
    case ALWAYS_OPEN_TYPE: {
      const FilePath::StringType extension =
          file_util::GetFileExtensionFromPath(download_->full_path());
      download_->manager()->OpenFilesOfExtension(
          extension, !ItemIsChecked(ALWAYS_OPEN_TYPE));
      break;
    }
    case CANCEL:
      model_->CancelTask();
      break;
    default:
      NOTREACHED();
  }
}
