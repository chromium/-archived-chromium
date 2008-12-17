// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingService on the UI
// thread when we've determined that a page is malicious. The operation of the
// blocking page occurs on the UI thread, where it waits for the user to make a
// decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingService so that we can cancel the request for the new page, or
// or allow it to continue.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include "base/logging.h"
#include "chrome/browser/interstitial_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "googleurl/src/gurl.h"

class MessageLoop;
class TabContents;
class NavigationController;

class SafeBrowsingBlockingPage : public InterstitialPage {
 public:
  SafeBrowsingBlockingPage(SafeBrowsingService* service,
                           const SafeBrowsingService::BlockingPageParam& param);
  virtual ~SafeBrowsingBlockingPage();

  // InterstitialPage method:
  virtual std::string GetHTMLContents();
  virtual void InterstitialClosed();

 protected:
  // InterstitialPage method:
  virtual void CommandReceived(const std::string& command);

 private:
  // Tells the SafeBrowsingService that the handling of the current page is
  // done.
  void NotifyDone();

 private:
  // For reporting back user actions.
  SafeBrowsingService* sb_service_;
  SafeBrowsingService::Client* client_;
  MessageLoop* report_loop_;
  SafeBrowsingService::UrlCheckResult result_;

  // For determining which tab to block (note that we need this even though we
  // have access to the tab as when the interstitial is showing, retrieving the
  // tab RPH and RV id would return the ones of the interstitial, not the ones
  // for the page containing the malware).
  // TODO(jcampan): when we refactor the interstitial to run as a separate view
  //                that does not interact with the WebContents as much, we can
  //                get rid of these.
  int render_process_host_id_;
  int render_view_id_;

  // Inform the SafeBrowsingService whether we are continuing with this page
  // load or going back to the previous page.
  bool proceed_;

  // Whether we have notify the SafeBrowsingService yet that a decision had been
  // made whether to proceed or block the unsafe resource.
  bool did_notify_;

  // Whether the flagged resource is the main page (or a sub-resource is false).
  bool is_main_frame_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
