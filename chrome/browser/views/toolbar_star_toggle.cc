// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/toolbar_star_toggle.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/views/bookmark_bubble_view.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/resource_bundle.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"

using base::TimeTicks;

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
      ignore_click_(false) {
}

void ToolbarStarToggle::ShowStarBubble(const GURL& url, bool newly_bookmarked) {
  gfx::Point star_location;
  views::View::ConvertPointToScreen(this, &star_location);
  // Shift the x location by 1 as visually the center of the star appears 1
  // pixel to the right. By doing this bubble arrow points to the center
  // of the star.
  gfx::Rect star_bounds(star_location.x() + 1, star_location.y(), width(),
                        height());
  HWND parent_hwnd =
      reinterpret_cast<HWND>(host_->browser()->window()->GetNativeHandle());
  BookmarkBubbleView::Show(parent_hwnd, star_bounds, this, host_->profile(),
                           url, newly_bookmarked);
}

bool ToolbarStarToggle::OnMousePressed(const views::MouseEvent& e) {
  ignore_click_ = ((TimeTicks::Now() - bubble_closed_time_).InMilliseconds() <
                   kDisallowClickMS);
  return ToggleButton::OnMousePressed(e);
}

void ToolbarStarToggle::OnMouseReleased(const views::MouseEvent& e,
                                        bool canceled) {
  ToggleButton::OnMouseReleased(e, canceled);
  ignore_click_ = false;
}

void ToolbarStarToggle::OnDragDone() {
  ToggleButton::OnDragDone();
  ignore_click_ = false;
}

void ToolbarStarToggle::NotifyClick(int mouse_event_flags) {
  if (!ignore_click_ && !BookmarkBubbleView::IsShowing())
    ToggleButton::NotifyClick(mouse_event_flags);
}

SkBitmap ToolbarStarToggle::GetImageToPaint() {
  if (BookmarkBubbleView::IsShowing()) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    return *rb.GetBitmapNamed(IDR_STARRED_P);
  }
  return Button::GetImageToPaint();
}

void ToolbarStarToggle::InfoBubbleClosing(InfoBubble* info_bubble,
                                          bool closed_by_escape) {
  SchedulePaint();
  bubble_closed_time_ = TimeTicks::Now();
}

bool ToolbarStarToggle::CloseOnEscape() {
  return true;
}
