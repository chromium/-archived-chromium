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

#include "chrome/browser/views/toolbar_star_toggle.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chrome_frame.h"
#include "chrome/browser/views/bookmark_bubble_view.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/resource_bundle.h"
#include "googleurl/src/gurl.h"

// The amount of time (in milliseconds) between when the bubble closes and when
// pressing on the button again does something. Yes, this is a hackish. I tried
// many different options, all to no avail:
// . Keying off mouse activation: this didn't work as there is no way to know
//   which window receives the activation. Additionally once the mouse
//   activation occurs we have no way to tie the next mouse event to the mouse
//   activation.
// . Watching all events as we dispatch them in the MessageLoop. Mouse
//   activation isn't an observable event though.
// Ideally we could use mouse capture for this, but we can't use mouse capture
// with the bubble because it has other native windows.
static const int64 kDisallowClickMS = 40;

ToolbarStarToggle::ToolbarStarToggle(BrowserToolbarView* host)
    : host_(host),
      ignore_click_(false),
      is_bubble_showing_(false) {
}

void ToolbarStarToggle::ShowStarBubble(const GURL& url, bool newly_bookmarked) {
  if (is_bubble_showing_) {
    // Don't show if we're already showing the bubble.
    return;
  }

  CPoint star_location(0, 0);
  ChromeViews::View::ConvertPointToScreen(this, &star_location);
  // Shift the x location by 1 as visually the center of the star appears 1
  // pixel to the right. By doing this bubble arrow points to the center
  // of the star.
  gfx::Rect star_bounds(star_location.x + 1, star_location.y, GetWidth(),
                        GetHeight());
  BookmarkBubbleView::Show(host_->browser()->GetTopLevelHWND(), star_bounds,
                           this, host_->profile(), url, newly_bookmarked);
  is_bubble_showing_ = true;
}

bool ToolbarStarToggle::OnMousePressed(const ChromeViews::MouseEvent& e) {
  ignore_click_ = ((TimeTicks::Now() - bubble_closed_time_).InMilliseconds() <
                   kDisallowClickMS);
  return ToggleButton::OnMousePressed(e);
}

void ToolbarStarToggle::OnMouseReleased(const ChromeViews::MouseEvent& e,
                                        bool canceled) {
  ToggleButton::OnMouseReleased(e, canceled);
  ignore_click_ = false;
}

void ToolbarStarToggle::OnDragDone() {
  ToggleButton::OnDragDone();
  ignore_click_ = false;
}

void ToolbarStarToggle::NotifyClick(int mouse_event_flags) {
  if (!ignore_click_ && !is_bubble_showing_)
    ToggleButton::NotifyClick(mouse_event_flags);
}

SkBitmap ToolbarStarToggle::GetImageToPaint() {
  if (is_bubble_showing_) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    return *rb.GetBitmapNamed(IDR_STARRED_P);
  }
  return Button::GetImageToPaint();
}

void ToolbarStarToggle::InfoBubbleClosing(InfoBubble* info_bubble) {
  is_bubble_showing_ = false;
  SchedulePaint();
  bubble_closed_time_ = TimeTicks::Now();
}

bool ToolbarStarToggle::CloseOnEscape() {
  return true;
}
