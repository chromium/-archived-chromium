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

#include "chrome/browser/views/importer_lock_view.h"

#include "chrome/browser/importer.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/label.h"

#include "generated_resources.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;

// Default size of the dialog window.
static const int kDefaultWindowWidth = 320;
static const int kDefaultWindowHeight = 100;

ImporterLockView::ImporterLockView(ImporterHost* host)
    : dialog_(NULL),
      description_label_(NULL),
      importer_host_(host) {
  description_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_IMPORTER_LOCK_TEXT));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  AddChildView(description_label_);
}

ImporterLockView::~ImporterLockView() {
}

void ImporterLockView::GetPreferredSize(CSize *out) {
  out->cx = kDefaultWindowWidth;
  out->cy = kDefaultWindowHeight;
}

void ImporterLockView::Layout() {
  description_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
      kDefaultWindowWidth - 2 * kPanelHorizMargin,
      kDefaultWindowHeight - 2 * kPanelVertMargin);
}

std::wstring ImporterLockView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_IMPORTER_LOCK_OK);
  } else if (button == DIALOGBUTTON_CANCEL) {
    return l10n_util::GetString(IDS_IMPORTER_LOCK_CANCEL);
  }
  return std::wstring();
}

bool ImporterLockView::IsModal() const {
  return true;
}

std::wstring ImporterLockView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_IMPORTER_LOCK_TITLE);
}

bool ImporterLockView::Accept() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, true));
  return true;
}

bool ImporterLockView::Cancel() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, false));
  return true;
}
