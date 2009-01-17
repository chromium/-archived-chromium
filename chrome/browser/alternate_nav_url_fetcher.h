// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
#define CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_

#include <string>

#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"

class NavigationController;

// Attempts to get the HEAD of a host name and displays an info bar if the
// request was successful. This is used for single-word queries where we can't
// tell if the entry was a search or an intranet hostname. The autocomplete bar
// assumes it's a query and issues an AlternateNavURLFetcher to display a "did
// you mean" infobar suggesting a navigation.
//
// The memory management of this object is a bit tricky. The location bar view
// will create us and be responsible for us until we attach as an observer
// after a pending load starts (it will delete us if this doesn't happen).
// Once this pending load starts, we're responsible for deleting ourselves.
// We'll do this when the load commits, or when the navigation controller
// itself is deleted.
class AlternateNavURLFetcher : public NotificationObserver,
                               public URLFetcher::Delegate,
                               public LinkInfoBarDelegate {
 public:
  enum State {
    NOT_STARTED,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
  };

  explicit AlternateNavURLFetcher(const GURL& alternate_nav_url);

  State state() const { return state_; }

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

  // LinkInfoBarDelegate
  virtual std::wstring GetMessageTextWithOffset(size_t* link_offset) const;
  virtual std::wstring GetLinkText() const;
  virtual SkBitmap* GetIcon() const;
  virtual bool LinkClicked(WindowOpenDisposition disposition);
  virtual void InfoBarClosed();

 private:
  // Displays the infobar if all conditions are met (the page has loaded and
  // the fetch of the alternate URL succeeded).
  void ShowInfobarIfPossible();

  GURL alternate_nav_url_;
  scoped_ptr<URLFetcher> fetcher_;
  NavigationController* controller_;
  State state_;
  bool navigated_to_entry_;
  
  // The TabContents the InfoBarDelegate was added to.
  TabContents* infobar_contents_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavURLFetcher);
};

#endif  // CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
