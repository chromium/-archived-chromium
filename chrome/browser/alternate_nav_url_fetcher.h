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

#ifndef CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H__
#define CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H__

#include <string>

#include "chrome/browser/url_fetcher.h"
#include "chrome/common/notification_service.h"

class NavigationController;

// Attempts to get the HEAD of a host name and displays an info bar if the
// request was successful. This is used for single-word queries where we can't
// tell if the entry was a search or an intranet hostname. The autocomplete bar
// assumes it's a query and issues an AlternateNavURLFetcher to display a "did
// you mean" infobar suggesting a navigation.
class AlternateNavURLFetcher : public NotificationObserver,
                               public URLFetcher::Delegate {
 public:
  enum State {
    NOT_STARTED,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
  };

  explicit AlternateNavURLFetcher(const std::wstring& alternate_nav_url);
  virtual ~AlternateNavURLFetcher();

  State state() const { return state_; }

  // Called by the NavigationController when it successfully navigates to the
  // entry for which the fetcher is looking up an alternative.
  // NOTE: This can be theoretically called multiple times, if multiple
  // navigations with the same unique ID succeed.
  void OnNavigatedToEntry();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  void ShowInfobar();

  std::wstring alternate_nav_url_;
  scoped_ptr<URLFetcher> fetcher_;
  NavigationController* controller_;
  State state_;
  bool navigated_to_entry_;

  DISALLOW_EVIL_CONSTRUCTORS(AlternateNavURLFetcher);
};

#endif  // CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H__
