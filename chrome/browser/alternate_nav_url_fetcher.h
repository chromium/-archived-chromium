// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

