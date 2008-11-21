// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobar_delegate.h"

#include "base/logging.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

bool AlertInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  AlertInfoBarDelegate* alert_delegate = delegate->AsAlertInfoBarDelegate();
  if (!alert_delegate)
    return false;

  return alert_delegate->GetMessageText() == GetMessageText();
}

// ConfirmInfoBarDelegate, public: ---------------------------------------------

std::wstring ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_OK);
  if (button == BUTTON_CANCEL)
    return l10n_util::GetString(IDS_CANCEL);
  NOTREACHED();
  return std::wstring();
}

// SimpleAlertInfoBarDelegate, public: -----------------------------------------

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    const std::wstring& message, SkBitmap* icon)
    : message_(message),
      icon_(icon) {
}

// SimpleAlertInfoBarDelegate, AlertInfoBarDelegate implementation: ------------

std::wstring SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

SkBitmap* SimpleAlertInfoBarDelegate::GetIcon() const {
  return icon_;
}

void SimpleAlertInfoBarDelegate::InfoBarClosed() {
  delete this;
}
