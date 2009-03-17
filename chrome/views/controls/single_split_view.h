// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
#define CHROME_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_

#include "chrome/views/view.h"

namespace views {

// SingleSplitView lays out two views horizontally. A splitter exists between
// the two views that the user can drag around to resize the views.
class SingleSplitView : public views::View {
 public:
  SingleSplitView(View* leading, View* trailing);

  virtual void Layout();

  // SingleSplitView's preferred size is the sum of the preferred widths
  // and the max of the heights.
  virtual gfx::Size GetPreferredSize();

  // Overriden to return a resize cursor when over the divider.
  virtual HCURSOR GetCursorForPoint(Event::EventType event_type, int x, int y);

  void set_divider_x(int divider_x) { divider_x_ = divider_x; }
  int divider_x() { return divider_x_; }

 protected:
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);

 private:
  // Returns true if |x| is over the divider.
  bool IsPointInDivider(int x);

  // Used to track drag info.
  struct DragInfo {
    // The initial coordinate of the mouse when the user started the drag.
    int initial_mouse_x;
    // The initial position of the divider when the user started the drag.
    int initial_divider_x;
  };

  DragInfo drag_info_;

  // Position of the divider.
  int divider_x_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitView);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
