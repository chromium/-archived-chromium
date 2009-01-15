// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

// InfoBarDelegate: ------------------------------------------------------------

bool InfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  bool is_reload =
      PageTransition::StripQualifier(details.entry->transition_type()) ==
          PageTransition::RELOAD;
  return is_reload || (contents_unique_id_ != details.entry->unique_id());
}

InfoBarDelegate::InfoBarDelegate(TabContents* contents)
    : contents_unique_id_(0) {
  if (contents)
    StoreActiveEntryUniqueID(contents);
}

void InfoBarDelegate::StoreActiveEntryUniqueID(TabContents* contents) {
  NavigationEntry* active_entry = contents->controller()->GetActiveEntry();
  contents_unique_id_ = active_entry ? active_entry->unique_id() : 0;
}

// AlertInfoBarDelegate: -------------------------------------------------------

bool AlertInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  AlertInfoBarDelegate* alert_delegate = delegate->AsAlertInfoBarDelegate();
  if (!alert_delegate)
    return false;

  return alert_delegate->GetMessageText() == GetMessageText();
}

AlertInfoBarDelegate::AlertInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

// LinkInfoBarDelegate: --------------------------------------------------------

LinkInfoBarDelegate::LinkInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

// ConfirmInfoBarDelegate: -----------------------------------------------------

std::wstring ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_OK);
  if (button == BUTTON_CANCEL)
    return l10n_util::GetString(IDS_CANCEL);
  NOTREACHED();
  return std::wstring();
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate(TabContents* contents)
    : AlertInfoBarDelegate(contents) {
}

// SimpleAlertInfoBarDelegate: -------------------------------------------------

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    TabContents* contents,
    const std::wstring& message,
    SkBitmap* icon)
    : AlertInfoBarDelegate(contents),
      message_(message),
      icon_(icon) {
}

std::wstring SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

SkBitmap* SimpleAlertInfoBarDelegate::GetIcon() const {
  return icon_;
}

void SimpleAlertInfoBarDelegate::InfoBarClosed() {
  delete this;
}
