// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SCROLLBAR_SCROLLBAR_H_
#define VIEWS_CONTROLS_SCROLLBAR_SCROLLBAR_H_

#include "views/view.h"
#include "views/event.h"

namespace views {

class ScrollBar;

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBarController
//
// ScrollBarController defines the method that should be implemented to
// receive notification from a scrollbar
//
/////////////////////////////////////////////////////////////////////////////
class ScrollBarController {
 public:

  // Invoked by the scrollbar when the scrolling position changes
  // This method typically implements the actual scrolling.
  //
  // The provided position is expressed in pixels. It is the new X or Y
  // position which is in the GetMinPosition() / GetMaxPosition range.
  virtual void ScrollToPosition(ScrollBar* source, int position) = 0;

  // Returns the amount to scroll. The amount to scroll may be requested in
  // two different amounts. If is_page is true the 'page scroll' amount is
  // requested. The page scroll amount typically corresponds to the
  // visual size of the view. If is_page is false, the 'line scroll' amount
  // is being requested. The line scroll amount typically corresponds to the
  // size of one row/column.
  //
  // The return value should always be positive. A value <= 0 results in
  // scrolling by a fixed amount.
  virtual int GetScrollIncrement(ScrollBar* source,
                                 bool is_page,
                                 bool is_positive) = 0;
};

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBar
//
// A View subclass to wrap to implement a ScrollBar. Our current windows
// version simply wraps a native windows scrollbar.
//
// A scrollbar is either horizontal or vertical
//
/////////////////////////////////////////////////////////////////////////////
class ScrollBar : public View {
 public:
  virtual ~ScrollBar();

  // Return whether this scrollbar is horizontal
  bool IsHorizontal() const;

  // Set / Get the controller
  void SetController(ScrollBarController* controller);
  ScrollBarController* GetController() const;

  // Update the scrollbar appearance given a viewport size, content size and
  // current position
  virtual void Update(int viewport_size, int content_size, int current_pos);

  // Return the max and min positions
  int GetMaxPosition() const;
  int GetMinPosition() const;

  // Returns the position of the scrollbar.
  virtual int GetPosition() const = 0;

  // Get the width or height of this scrollbar, for use in layout calculations.
  // For a vertical scrollbar, this is the width of the scrollbar, likewise it
  // is the height for a horizontal scrollbar.
  virtual int GetLayoutSize() const = 0;

 protected:
  // Create new scrollbar, either horizontal or vertical. These are protected
  // since you need to be creating either a NativeScrollBar or a
  // BitmapScrollBar.
  ScrollBar(bool is_horiz);

 private:
  const bool is_horiz_;

  // Current controller
  ScrollBarController* controller_;

  // properties
  int max_pos_;
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_SCROLLBAR_SCROLLBAR_H_
