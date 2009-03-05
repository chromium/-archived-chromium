// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_DROP_INFO_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_DROP_INFO_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/timer.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"

namespace views {
class DropTargetEvent;
}

// BookmarkDropInfo is a pure virtual class that provides auto-scrolling
// behavior and a handful of fields used for managing a bookmark drop.
// BookmarkDropInfo is used by both BookmarkTableView and
// BookmarksFolderTreeView.
class BookmarkDropInfo {
 public:
  BookmarkDropInfo(gfx::NativeWindow hwnd, int top_margin);
  virtual ~BookmarkDropInfo() {}

  // Invoke this from OnDragUpdated. It resets source_operations,
  // is_control_down, last_y and updates the autoscroll timer as necessary.
  void Update(const views::DropTargetEvent& event);

  // Data from the drag.
  void SetData(const BookmarkDragData& data) { data_ = data; }
  BookmarkDragData& data() { return data_; }

  // Value of event.GetSourceOperations when Update was last invoked.
  int source_operations() const { return source_operations_; }

  // Whether the control key was down last time Update was invoked.
  bool is_control_down() const { return is_control_down_; }

  // Y position of the event last passed to Update.
  int last_y() { return last_y_; }

  // The drop operation that should occur. This is not updated by
  // BookmarkDropInfo, but provided for subclasses.
  void set_drop_operation(int drop_operation) {
    drop_operation_ = drop_operation;
  }
  int drop_operation() const { return drop_operation_; }

 protected:
  // Invoked if we autoscroll. When invoked subclasses need to determine
  // whether the drop is valid again as what is under the mouse has likely
  // scrolled.
  virtual void Scrolled() = 0;

 private:
  // Invoked from the timer. Scrolls up/down a line.
  void Scroll();

  BookmarkDragData data_;

  int source_operations_;

  bool is_control_down_;

  int last_y_;

  int drop_operation_;

  gfx::NativeWindow wnd_;

  // Margin in addition to views::kAutoscrollSize that the mouse is allowed to
  // be over before we autoscroll.
  int top_margin_;

  // When autoscrolling this determines if we're scrolling up or down.
  bool scroll_up_;

  // Used when autoscrolling.
  base::RepeatingTimer<BookmarkDropInfo> scroll_timer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDropInfo);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_DROP_INFO_H_
