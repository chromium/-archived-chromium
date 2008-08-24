// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H__
#define CHROME_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H__

// The InterstitialPageDelegate interface allows implementing classes to take
// action when different events happen while an interstitial page is shown.
// It is passed to the WebContents::ShowInterstitial() method.

class InterstitialPageDelegate {
 public:
  // Notification that the interstitial page was closed.  This is the last call
  // that the delegate gets.
  virtual void InterstitialClosed() = 0;

  // The tab showing this interstitial page is navigating back.  If this returns
  // false, the default back behavior is executed (navigating to the previous
  // navigation entry).  If this returns true, no navigation is performed (it
  // is assumed the implementation took care of the navigation).
  virtual bool GoBack() = 0;
};

#endif  // CHROME_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H__


