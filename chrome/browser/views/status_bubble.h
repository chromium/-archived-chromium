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

#ifndef CHROME_BROWSER_VIEWS_STATUS_BUBBLE_H__
#define CHROME_BROWSER_VIEWS_STATUS_BUBBLE_H__

#include "base/gfx/rect.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/views/view_container.h"

class GURL;

// StatusBubble displays a bubble of text that fades in, hovers over the
// browser chrome and fades away when not needed. It is primarily designed
// to allow users to see where hovered links point to.
class StatusBubble {
 public:
  explicit StatusBubble(ChromeViews::ViewContainer* frame);
  ~StatusBubble();

  // Sets the bubble contents to a specific string and causes the bubble
  // to display immediately. Subsequent empty SetURL calls (typically called
  // when the cursor exits a link) will set the status bubble back to its
  // status text. To hide the status bubble again, either call SetStatus
  // with an empty string, or call Hide().
  void SetStatus(const std::wstring& status);

  // Sets the bubble text to a URL - if given a non-empty URL, this will cause
  // the bubble to fade in and remain open until given an empty URL or until
  // the Hide() method is called. languages is the value of Accept-Language
  // to determine what characters are understood by a user.
  void SetURL(const GURL& url, const std::wstring& languages);

  // Clear the URL and begin the fadeout of the bubble if not status text
  // needs to be displayed.
  void ClearURL();

  // Skip the fade and instant-hide the bubble.
  void Hide();

  // Called when the user's mouse has moved over web content.
  void MouseMoved();

  // Set the bounds of the bubble relative to the browser window.
  void SetBounds(int x, int y, int w, int h);

  // Reposition the bubble - as we are using a WS_POPUP for the bubble,
  // we have to manually position it when the browser window moves.
  void Reposition();

 private:
  class StatusView;

  // Initializes the popup and view.
  void Init();

  // Attempt to move the status bubble out of the way of the cursor, allowing
  // users to see links in the region normally occupied by the status bubble.
  void AvoidMouse();

  // The status text we want to display when there are no URLs to display.
  std::wstring status_text_;

  // The url we want to display when there is not status text to display.
  std::wstring url_text_;

  // Position relative to the parent window.
  CPoint position_;
  CSize size_;

  // How vertically offset the bubble is from its root position_.
  int offset_;

  // We use a HWND for the popup so that it may float above any HWNDs in our
  // UI (the location bar, for example).
  ChromeViews::HWNDViewContainer* popup_;
  double opacity_;

  ChromeViews::ViewContainer* frame_;
  StatusView* view_;

  DISALLOW_EVIL_CONSTRUCTORS(StatusBubble);
};

#endif  // CHROME_BROWSER_VIEWS_STATUS_BUBBLE_H__
