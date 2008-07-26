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

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_

#include "base/time.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/views/button.h"

class BrowserToolbarView;
class GURL;

// ToolbarStarToggle is used for the star button on the toolbar, allowing the
// user to star the current page. ToolbarStarToggle manages showing the
// InfoBubble and rendering the appropriate state while the bubble is visible.

class ToolbarStarToggle : public ChromeViews::ToggleButton,
                          public InfoBubbleDelegate {
 public:
  explicit ToolbarStarToggle(BrowserToolbarView* host);

  // If the bubble isn't showing, shows it.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  bool is_bubble_showing() const { return is_bubble_showing_; }

  // Overridden to update ignore_click_ based on whether the mouse was clicked
  // quickly after the bubble was hidden.
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);

  // Overridden to set ignore_click_ to false.
  virtual void OnMouseReleased(const ChromeViews::MouseEvent& e,
                               bool canceled);
  virtual void OnDragDone();

  // Only invokes super if ignore_click_ is true and the bubble isn't showing.
  virtual void NotifyClick(int mouse_event_flags);

 protected:
  // Overridden to so that we appear pressed while the bubble is showing.
  virtual SkBitmap GetImageToPaint();

 private:
  // InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble);
  virtual bool CloseOnEscape();

  // Contains us.
  BrowserToolbarView* host_;

  // Time the bubble last closed.
  TimeTicks bubble_closed_time_;

  // If true NotifyClick does nothing. This is set in OnMousePressed based on
  // the amount of time between when the bubble clicked and now.
  bool ignore_click_;

  // Is the bubble showing?
  bool is_bubble_showing_;

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarStarToggle);
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
