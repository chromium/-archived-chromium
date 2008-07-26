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
  NotificationService::current()->AddObserver(this,
      NOTIFY_NAVIGATION_STATE_CHANGED, NotificationService::AllSources());
}

AlternateNavURLFetcher::~AlternateNavURLFetcher() {
  if (state_ == NOT_STARTED) {
    // Never caught the NavigationController notification.
    NotificationService::current()->RemoveObserver(this,
        NOTIFY_NAVIGATION_STATE_CHANGED, NotificationService::AllSources());
  }  // Otherwise, Observe() removed the observer already.
}

void AlternateNavURLFetcher::OnNavigatedToEntry() {
  if (navigated_to_entry_)
    return;
  navigated_to_entry_ = true;
  if (state_ == SUCCEEDED)
    ShowInfobar();
}

void AlternateNavURLFetcher::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  controller_ = Source<NavigationController>(source).ptr();
  if (!controller_->GetPendingEntry())
    return;  // No entry to attach ourselves to.
  controller_->SetAlternateNavURLFetcher(this);

  NotificationService::current()->RemoveObserver(this,
      NOTIFY_NAVIGATION_STATE_CHANGED, NotificationService::AllSources());

  DCHECK_EQ(NOT_STARTED, state_);
  state_ = IN_PROGRESS;
  fetcher_.reset(new URLFetcher(GURL(alternate_nav_url_), URLFetcher::HEAD,
                                this));
  fetcher_->set_request_context(controller_->profile()->GetRequestContext());
  fetcher_->Start();
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
    if (navigated_to_entry_)
      ShowInfobar();
  } else {
    state_ = FAILED;
  }
}

void AlternateNavURLFetcher::ShowInfobar() {
  const NavigationEntry* const entry = controller_->GetActiveEntry();
  DCHECK(entry);
  if (entry->GetType() != TAB_CONTENTS_WEB)
    return;
  TabContents* tab_contents =
      controller_->GetTabContents(TAB_CONTENTS_WEB);
  DCHECK(tab_contents);
  WebContents* web_contents = tab_contents->AsWebContents();
  // The infobar will auto-expire this view on the next user-initiated
  // navigation, so we don't need to keep track of it.
  web_contents->GetInfoBarView()->AddChildView(new InfoBarAlternateNavURLView(
      alternate_nav_url_));
}
