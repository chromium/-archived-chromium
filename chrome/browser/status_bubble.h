// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_BUBBLE_H_
#define CHROME_BROWSER_STATUS_BUBBLE_H_

#include <string>

class GURL;

////////////////////////////////////////////////////////////////////////////////
// StatusBubble interface
//  An interface implemented by an object providing the status display area of
//  the browser window.
//
class StatusBubble {
 public:
  virtual ~StatusBubble() { };

  // Sets the bubble contents to a specific string and causes the bubble
  // to display immediately. Subsequent empty SetURL calls (typically called
  // when the cursor exits a link) will set the status bubble back to its
  // status text. To hide the status bubble again, either call SetStatus
  // with an empty string, or call Hide().
  virtual void SetStatus(const std::wstring& status) = 0;

  // Sets the bubble text to a URL - if given a non-empty URL, this will cause
  // the bubble to fade in and remain open until given an empty URL or until
  // the Hide() method is called. languages is the value of Accept-Language
  // to determine what characters are understood by a user.
  virtual void SetURL(const GURL& url, const std::wstring& languages) = 0;

  // Skip the fade and instant-hide the bubble.
  virtual void Hide() = 0;

  // Called when the user's mouse has moved over web content. This is used to
  // determine when the status area should move out of the way of the user's
  // mouse. This may be windows specific pain due to the way messages are
  // processed for child HWNDs.
  virtual void MouseMoved() = 0;

  // Called when the download shelf becomes visible or invisible.
  // This is used by to ensure that the status bubble does not obscure
  // the download shelf, when it is visible.
  virtual void UpdateDownloadShelfVisibility(bool visible) = 0;
};

#endif  // #ifndef CHROME_BROWSER_STATUS_BUBBLE_H_
