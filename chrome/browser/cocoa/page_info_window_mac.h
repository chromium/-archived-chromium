// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
#define CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_

#include "chrome/browser/history/history.h"
#include "chrome/browser/page_info_window.h"

class CancelableRequestConsumerBase;
class Profile;
@class PageInfoWindowController;

namespace base {
class Time;
}

class PageInfoWindowMac : public PageInfoWindow {
 public:
  PageInfoWindowMac(PageInfoWindowController* controller);
  virtual ~PageInfoWindowMac();

  // This is the main initializer that creates the window.
  virtual void Init(Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    NavigationEntry::PageType page_type,
                    bool show_history,
                    gfx::NativeView parent);

  virtual void Show();

  // Shows various information for the specified certificate in a new dialog.
  // The argument is ignored here and we use the |cert_id_| member that was
  // passed to us in Init().
  virtual void ShowCertDialog(int);

 private:
  void OnGotVisitCountToHost(HistoryService::Handle handle,
                             bool found_visits,
                             int count,
                             base::Time first_visit);

  CancelableRequestConsumer request_consumer_;  // Used for getting visit count.
  PageInfoWindowController* controller_;  // WEAK, owns us.

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowMac);
};

#endif  // CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
