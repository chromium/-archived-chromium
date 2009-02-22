// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alternate_nav_url_fetcher.h"

#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"

AlternateNavURLFetcher::AlternateNavURLFetcher(
    const GURL& alternate_nav_url)
    : LinkInfoBarDelegate(NULL),
      alternate_nav_url_(alternate_nav_url),
      controller_(NULL), 
      state_(NOT_STARTED),
      navigated_to_entry_(false),
      infobar_contents_(NULL) {
  registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
                 NotificationService::AllSources());
}

void AlternateNavURLFetcher::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_PENDING:
      controller_ = Source<NavigationController>(source).ptr();
      DCHECK(controller_->GetPendingEntry());

      // Unregister for this notification now that we're pending, and start
      // listening for the corresponding commit. We also need to listen for the
      // tab close command since that means the load will never commit!
      registrar_.Remove(this, NotificationType::NAV_ENTRY_PENDING,
                        NotificationService::AllSources());
      registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                     Source<NavigationController>(controller_));

      DCHECK_EQ(NOT_STARTED, state_);
      state_ = IN_PROGRESS;
      fetcher_.reset(new URLFetcher(GURL(alternate_nav_url_), URLFetcher::HEAD,
                                    this));
      fetcher_->set_request_context(controller_->profile()->GetRequestContext());
      fetcher_->Start();
      break;

    case NotificationType::NAV_ENTRY_COMMITTED:
      // The page was navigated, we can show the infobar now if necessary.
      registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
                        Source<NavigationController>(controller_));
      navigated_to_entry_ = true;
      ShowInfobarIfPossible();
      break;

    default:
      NOTREACHED();
  }
}

void AlternateNavURLFetcher::OnURLFetchComplete(const URLFetcher* source,
                                                const GURL& url,
                                                const URLRequestStatus& status,
                                                int response_code,
                                                const ResponseCookies& cookies,
                                                const std::string& data) {
  DCHECK(fetcher_.get() == source);
  if (status.is_success() &&
      // HTTP 2xx, 401, and 407 all indicate that the target address exists.
      (((response_code / 100) == 2) ||
       (response_code == 401) || (response_code == 407))) {
    state_ = SUCCEEDED;
    ShowInfobarIfPossible();
  } else {
    state_ = FAILED;
  }
}

std::wstring AlternateNavURLFetcher::GetMessageTextWithOffset(
    size_t* link_offset) const {
  const std::wstring label = l10n_util::GetStringF(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, std::wstring(), link_offset);
  DCHECK(*link_offset != std::wstring::npos);
  return label;
}

std::wstring AlternateNavURLFetcher::GetLinkText() const {
  return UTF8ToWide(alternate_nav_url_.spec());
}

SkBitmap* AlternateNavURLFetcher::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_ALT_NAV_URL);
}

bool AlternateNavURLFetcher::LinkClicked(WindowOpenDisposition disposition) {
  infobar_contents_->OpenURL(
      alternate_nav_url_, GURL(), disposition,
      // Pretend the user typed this URL, so that navigating to
      // it will be the default action when it's typed again in
      // the future.
      PageTransition::TYPED);

  // We should always close, even if the navigation did not occur within this
  // TabContents.
  return true;
}

void AlternateNavURLFetcher::InfoBarClosed() {
  delete this;
}

void AlternateNavURLFetcher::ShowInfobarIfPossible() {
  if (!navigated_to_entry_ || state_ != SUCCEEDED)
    return;

  infobar_contents_ = controller_->active_contents();
  StoreActiveEntryUniqueID(infobar_contents_);
  // We will be deleted when the InfoBar is destroyed. (See InfoBarClosed).
  infobar_contents_->AddInfoBar(this);
}
