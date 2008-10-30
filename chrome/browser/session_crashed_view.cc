// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/session_crashed_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/session_restore.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"

#include "chromium_strings.h"
#include "generated_resources.h"

SessionCrashedView::SessionCrashedView(Profile* profile)
    : InfoBarConfirmView(
          l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_MESSAGE)),
      profile_(profile) {
  DCHECK(profile_);
  SetOKButtonLabel(
      l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON));
  RemoveCancelButton();
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetBitmapNamed(IDR_INFOBAR_RESTORE_SESSION));
}

SessionCrashedView::~SessionCrashedView() {
}

void SessionCrashedView::OKButtonPressed() {
  // Restore the session.
  SessionRestore::RestoreSession(profile_, NULL, false, true, false,
                                 std::vector<GURL>());

  // Close the info bar.
  InfoBarConfirmView::OKButtonPressed();
  // NOTE: OKButtonPressed eventually results in deleting us.
}

