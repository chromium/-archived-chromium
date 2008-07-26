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

#include "chrome/browser/session_crashed_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/session_restore.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
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
  SessionRestore::RestoreSession(profile_, false, true, false,
                                 std::vector<GURL>());

  // Close the info bar.
  InfoBarConfirmView::OKButtonPressed();
  // NOTE: OKButtonPressed eventually results in deleting us.
}
