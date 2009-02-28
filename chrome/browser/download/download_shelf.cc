// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/metrics/user_metrics.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

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
