// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_

#include "base/time.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/views/controls/button/image_button.h"

class BrowserToolbarView;
class GURL;

// ToolbarStarToggle is used for the star button on the toolbar, allowing the
// user to star the current page. ToolbarStarToggle manages showing the
// InfoBubble and rendering the appropriate state while the bubble is visible.

class ToolbarStarToggle : public views::ToggleImageButton,
                          public InfoBubbleDelegate {
 public:
  ToolbarStarToggle(views::ButtonListener* listener, BrowserToolbarView* host);

  // If the bubble isn't showing, shows it.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  // Overridden to update ignore_click_ based on whether the mouse was clicked
  // quickly after the bubble was hidden.
  virtual bool OnMousePressed(const views::MouseEvent& e);

  // Overridden to set ignore_click_ to false.
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual void OnDragDone();

 protected:
  // Only invokes super if ignore_click_ is true and the bubble isn't showing.
  virtual void NotifyClick(int mouse_event_flags);

  // Overridden to so that we appear pressed while the bubble is showing.
  virtual SkBitmap GetImageToPaint();

 private:
  // InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();

  // Contains us.
  BrowserToolbarView* host_;

  // Time the bubble last closed.
  base::TimeTicks bubble_closed_time_;

  // If true NotifyClick does nothing. This is set in OnMousePressed based on
  // the amount of time between when the bubble clicked and now.
  bool ignore_click_;

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarStarToggle);
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
