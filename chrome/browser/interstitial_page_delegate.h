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

