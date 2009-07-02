// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/status_bubble.h"
#include "googleurl/src/gurl.h"

class GURL;
namespace views {
class Widget;
}

// StatusBubble displays a bubble of text that fades in, hovers over the
// browser chrome and fades away when not needed. It is primarily designed
// to allow users to see where hovered links point to.
class StatusBubbleViews : public StatusBubble {
 public:
  // How wide the bubble's shadow is.
  static const int kShadowThickness;

  // The combined vertical padding above and below the text.
  static const int kTotalVerticalPadding = 7;

  // On hover, expand status bubble to accommodate long URL after this delay.
  static const int kExpandHoverDelay = 2000;

  explicit StatusBubbleViews(views::Widget* frame);
  ~StatusBubbleViews();

  // Reposition the bubble - as we are using a WS_POPUP for the bubble,
  // we have to manually position it when the browser window moves.
  void Reposition();

  // The bubble only has a preferred height: the sum of the height of
  // the font and kTotalVerticalPadding.
  gfx::Size GetPreferredSize();

  // Set the bounds of the bubble relative to the browser window.
  void SetBounds(int x, int y, int w, int h);

  // Set bubble to new width; for animation of expansion.
  void SetBubbleWidth(int width);

  // Overridden from StatusBubble:
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();
  virtual void UpdateDownloadShelfVisibility(bool visible);

 private:
  class StatusView;
  class StatusViewExpander;

  // Initializes the popup and view.
  void Init();

  // Attempt to move the status bubble out of the way of the cursor, allowing
  // users to see links in the region normally occupied by the status bubble.
  void AvoidMouse();

  // Expand bubble size to accommodate an abridged URL.
  void ExpandBubble();

  // Cancel all the expansions waiting in the timer.
  void CancelExpandTimer();

  // Get the standard width for a status bubble in the current frame size.
  int GetStandardStatusBubbleWidth();

  // Get the maximum possible width for a status bubble in the current
  // frame size.
  int GetMaxStatusBubbleWidth();

  // The status text we want to display when there are no URLs to display.
  std::wstring status_text_;

  // The url we want to display when there is no status text to display.
  // This string may be elided if the URL is too long to fit in status bubble.
  std::wstring url_text_;

  // The original url.  We need to keep this around to we can re-elide it to
  // dynamically fit the bubble if we need to expand it to show a url that
  // has been cut off.
  GURL url_;

  // Keep this around so we can elide the original url when we expand it.
  std::wstring languages_;

  // Position relative to the parent window.
  gfx::Point position_;
  gfx::Size size_;

  // How vertically offset the bubble is from its root position_.
  int offset_;

  // We use a HWND for the popup so that it may float above any HWNDs in our
  // UI (the location bar, for example).
  scoped_ptr<views::Widget> popup_;
  double opacity_;

  views::Widget* frame_;
  StatusView* view_;
  StatusViewExpander* expand_view_;

  // If the download shelf is visible, do not obscure it.
  bool download_shelf_is_visible_;

  // Is the bubble expanded?  If so, change size immediately.
  bool is_expanded_;

  // Times expansion of status bubble when URL is too long for standard width.
  ScopedRunnableMethodFactory<StatusBubbleViews> expand_timer_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusBubbleViews);
};

#endif  // CHROME_BROWSER_VIEWS_STATUS_BUBBLE_VIEWS_H_
