// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_STATUS_BUBBLE_MAC_H_
#define CHROME_BROWSER_COCOA_STATUS_BUBBLE_MAC_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "chrome/browser/status_bubble.h"

class GURL;
class StatusBubbleMacTest;

class StatusBubbleMac : public StatusBubble {
 public:
  StatusBubbleMac(NSWindow* parent);
  virtual ~StatusBubbleMac();

  // StatusBubble implementation.
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();
  virtual void UpdateDownloadShelfVisibility(bool visible);
  virtual void SetBubbleWidth(int width);

 private:
  friend class StatusBubbleMacTest;

  void SetStatus(NSString* status, bool is_url);

  // Construct the window/widget if it does not already exist. (Safe to call if
  // it does.)
  void Create();

  void FadeIn();
  void FadeOut();

  // The window we attach ourselves to.
  NSWindow* parent_;  // WEAK

  // The window we own.
  NSWindow* window_;

  // The status text we want to display when there are no URLs to display.
  NSString* status_text_;

  // The url we want to display when there is no status text to display.
  NSString* url_text_;

  // How vertically offset the bubble is from its root position.
  int offset_;
};

#endif  // #ifndef CHROME_BROWSER_COCOA_STATUS_BUBBLE_MAC_H_
