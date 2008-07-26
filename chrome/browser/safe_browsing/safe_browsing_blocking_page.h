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
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingService on the IO
// thread when we've determined that a page is malicious. The operation of the
// blocking page occurs on the UI thread, where it waits for the user to make a
// decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingService so that we can cancel the request for the new page, or
// or allow it to continue.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H__
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H__

#include "base/logging.h"
#include "chrome/browser/interstitial_page_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

class MessageLoop;
class TabContents;
class NavigationController;

class SafeBrowsingBlockingPage
    : public InterstitialPageDelegate,
      public base::RefCountedThreadSafe<SafeBrowsingBlockingPage>,
      public NotificationObserver {
 public:
  // Created and destroyed on the IO thread, operates on the UI thread.
  SafeBrowsingBlockingPage(SafeBrowsingService* service,
                           SafeBrowsingService::Client* client,
                           int render_process_host_id,
                           int render_view_id,
                           const GURL& url,
                           ResourceType::Type resource_type,
                           SafeBrowsingService::UrlCheckResult result);
  ~SafeBrowsingBlockingPage();

  // Display the page to the user. This method runs on the UI thread.
  void DisplayBlockingPage();

  // NotificationObserver interface, runs on the UI thread.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const GURL& url() { return url_; }
  int render_process_host_id() { return render_process_host_id_; }
  int render_view_id() { return render_view_id_; }
  SafeBrowsingService::UrlCheckResult result() { return result_; }

  // InterstitialPageDelegate methods:
  virtual void InterstitialClosed();
  virtual bool GoBack();

 private:
  // Handle user action for blocking page navigation choices.
  void Continue(const std::string& user_action);

  // Tell the SafeBrowsingService that the handling of the current page is done.
  void HandleClose();
  void NotifyDone();

 private:
  // For reporting back user actions.
  SafeBrowsingService* sb_service_;
  SafeBrowsingService::Client* client_;
  MessageLoop* report_loop_;

  // For determining which tab to block.
  int render_process_host_id_;
  int render_view_id_;

  GURL url_;
  SafeBrowsingService::UrlCheckResult result_;

  // Inform the SafeBrowsingService whether we are continuing with this page
  // load or going back to the previous page.
  bool proceed_;

  // Stored for use in the notification service, and are only used for their
  // pointer value, but not for calling methods on. This is done to allow us to
  // unregister as observers after the tab has gone (is NULL).
  TabContents* tab_;
  NavigationController* controller_;

  // Used for cleaning up after ourself.
  bool delete_pending_;

  // Whether the flagged resource is the main page (or a sub-resource is false).
  bool is_main_frame_;

  // Whether we have created a temporary navigation entry as part of showing
  // the blocking page.
  bool created_temporary_entry_;

  DISALLOW_EVIL_CONSTRUCTORS(SafeBrowsingBlockingPage);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H__