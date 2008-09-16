// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alternate_nav_url_fetcher.h"

#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/views/info_bar_alternate_nav_url_view.h"
#include "chrome/browser/web_contents.h"

AlternateNavURLFetcher::AlternateNavURLFetcher(
    const std::wstring& alternate_nav_url)
    : alternate_nav_url_(alternate_nav_url),
      controller_(NULL), 
      state_(NOT_STARTED),
      navigated_to_entry_(false) {
  registrar_.Add(this, NOTIFY_NAV_ENTRY_PENDING,
                 NotificationService::AllSources());
}

void AlternateNavURLFetcher::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_NAV_ENTRY_PENDING:
      controller_ = Source<NavigationController>(source).ptr();
      DCHECK(controller_->GetPendingEntry());

      // Unregister for this notification now that we're pending, and start
      // listening for the corresponding commit. We also need to listen for the
      // tab close command since that means the load will never commit!
      registrar_.Remove(this, NOTIFY_NAV_ENTRY_PENDING,
                        NotificationService::AllSources());
      registrar_.Add(this, NOTIFY_NAV_ENTRY_COMMITTED,
                     Source<NavigationController>(controller_));
      registrar_.Add(this, NOTIFY_TAB_CLOSED,
                     Source<NavigationController>(controller_));

      DCHECK_EQ(NOT_STARTED, state_);
      state_ = IN_PROGRESS;
      fetcher_.reset(new URLFetcher(GURL(alternate_nav_url_), URLFetcher::HEAD,
                                    this));
      fetcher_->set_request_context(controller_->profile()->GetRequestContext());
      fetcher_->Start();
      break;

    case NOTIFY_NAV_ENTRY_COMMITTED:
      // The page was navigated, we can show the infobar now if necessary.
      registrar_.Remove(this, NOTIFY_NAV_ENTRY_COMMITTED,
                        Source<NavigationController>(controller_));
      navigated_to_entry_ = true;
      ShowInfobarIfPossible();
      // DON'T DO ANYTHING AFTER HERE SINCE |this| MAY BE DELETED!
      break;

    case NOTIFY_TAB_CLOSED:
      // We listen either for tab closed or navigation committed to know when to
      // delete ourselves. Here, the tab closed, so we can just give up with
      // all our waiting for notifications and delete ourselves.
      delete this;
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
    // DON'T DO ANYTHING AFTER HERE SINCE |this| MAY BE DELETED!
  } else {
    state_ = FAILED;
  }
}

void AlternateNavURLFetcher::ShowInfobarIfPossible() {
  if (!navigated_to_entry_ || state_ != SUCCEEDED)
    return;

  const NavigationEntry* const entry = controller_->GetActiveEntry();
  DCHECK(entry);
  if (entry->tab_type() != TAB_CONTENTS_WEB)
    return;
  TabContents* tab_contents =
      controller_->GetTabContents(TAB_CONTENTS_WEB);
  DCHECK(tab_contents);
  WebContents* web_contents = tab_contents->AsWebContents();
  // The infobar will auto-expire this view on the next user-initiated
  // navigation, so we don't need to keep track of it.
  web_contents->GetInfoBarView()->AddChildView(new InfoBarAlternateNavURLView(
      alternate_nav_url_));

  // Now we're no longer referencing the navigation controller or the url fetch,
  // so our job is done.
  delete this;
}

