// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_

#include "base/gfx/rect.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/views/widget/widget.h"
#include "chrome/views/widget/widget_win.h"

class GURL;

// StatusBubble displays a bubble of text that fades in, hovers over the
// browser chrome and fades away when not needed. It is primarily designed
// to allow users to see where hovered links point to.
class StatusBubbleViews : public StatusBubble {
 public:
  explicit StatusBubbleViews(views::Widget* frame);
  ~StatusBubbleViews();

  // Set the bounds of the bubble relative to the browser window.
  void SetBounds(int x, int y, int w, int h);

  // Reposition the bubble - as we are using a WS_POPUP for the bubble,
  // we have to manually position it when the browser window moves.
  void Reposition();

  // Overridden from StatusBubble:
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();

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
  scoped_ptr<views::WidgetWin> popup_;
  double opacity_;

  views::Widget* frame_;
  StatusView* view_;

  DISALLOW_COPY_AND_ASSIGN(StatusBubbleViews);
};

#endif  // CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_
