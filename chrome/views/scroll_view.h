// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_SCROLL_VIEW_H__
#define CHROME_VIEWS_SCROLL_VIEW_H__

#include "chrome/views/scroll_bar.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ScrollView class
//
// A ScrollView is used to make any View scrollable. The view is added to
// a viewport which takes care of clipping.
//
// In this current implementation both horizontal and vertical scrollbars are
// added as needed.
//
// The scrollview supports keyboard UI and mousewheel.
//
/////////////////////////////////////////////////////////////////////////////

class ScrollView : public View,
                   public ScrollBarController {
 public:
  static const char* const kViewClassName;

  ScrollView();
  // Initialize with specific views. resize_corner is optional.
  ScrollView(ScrollBar* horizontal_scrollbar,
             ScrollBar* vertical_scrollbar,
             View* resize_corner);
  virtual ~ScrollView();

  // Set the contents. Any previous contents will be deleted. The contents
  // is the view that needs to scroll.
  void SetContents(View* a_view);
  View* GetContents() const;

  // Overridden to layout the viewport and scrollbars.
  virtual void Layout();

  // Returns the visible region of the content View.
  gfx::Rect GetVisibleRect() const;

  // Scrolls the minimum amount necessary to make the specified rectangle
  // visible, in the coordinates of the contents view. The specified rectangle
  // is constrained by the bounds of the contents view. This has no effect if
  // the contents have not been set.
  //
  // Client code should use ScrollRectToVisible, which invokes this
  // appropriately.
  void ScrollContentsRegionToBeVisible(int x, int y, int width, int height);

  // ScrollBarController.
  // NOTE: this is intended to be invoked by the ScrollBar, and NOT general
  // client code.
  // See also ScrollRectToVisible.
  virtual void ScrollToPosition(ScrollBar* source, int position);

  // Returns the amount to scroll relative to the visible bounds. This invokes
  // either GetPageScrollIncrement or GetLineScrollIncrement to determine the
  // amount to scroll. If the view returns 0 (or a negative value) a default
  // value is used.
  virtual int GetScrollIncrement(ScrollBar* source,
                                 bool is_page,
                                 bool is_positive);

  // Overridden to setup keyboard ui when the view hierarchy changes
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Keyboard events
  virtual bool OnKeyPressed(const KeyEvent& event);
  virtual bool OnMouseWheel(const MouseWheelEvent& e);

  virtual std::string GetClassName() const;

  // Retrieves the vertical scrollbar width.
  int GetScrollBarWidth() const;

  // Retrieves the horizontal scrollbar height.
  int GetScrollBarHeight() const;

  // Computes the visibility of both scrollbars, taking in account the view port
  // and content sizes.
  void ComputeScrollBarsVisibility(const gfx::Size& viewport_size,
                                   const gfx::Size& content_size,
                                   bool* horiz_is_shown,
                                   bool* vert_is_shown) const;

  ScrollBar* horizontal_scroll_bar() const { return horiz_sb_; }

  ScrollBar* vertical_scroll_bar() const { return vert_sb_; }

 private:
  // Initialize the ScrollView. resize_corner is optional.
  void Init(ScrollBar* horizontal_scrollbar,
            ScrollBar* vertical_scrollbar,
            View* resize_corner);

  // Shows or hides the scrollbar/resize_corner based on the value of
  // |should_show|.
  void SetControlVisibility(View* control, bool should_show);

  // Update the scrollbars positions given viewport and content sizes.
  void UpdateScrollBarPositions();

  // Make sure the content is not scrolled out of bounds
  void CheckScrollBounds();

  // Make sure the content is not scrolled out of bounds in one dimension
  int CheckScrollBounds(int viewport_size, int content_size, int current_pos);

  // The clipping viewport. Content is added to that view.
  View* viewport_;

  // The current contents
  View* contents_;

  // Horizontal scrollbar.
  ScrollBar* horiz_sb_;

  // Vertical scrollbar.
  ScrollBar* vert_sb_;

  // Resize corner.
  View* resize_corner_;

  DISALLOW_EVIL_CONSTRUCTORS(ScrollView);
};

// VariableRowHeightScrollHelper is intended for views that contain rows of
// varying height. To use a VariableRowHeightScrollHelper create one supplying
// a Controller and delegate GetPageScrollIncrement and GetLineScrollIncrement
// to the helper. VariableRowHeightScrollHelper calls back to the
// Controller to determine row boundaries.
class VariableRowHeightScrollHelper {
 public:
  // The origin and height of a row.
  struct RowInfo {
    RowInfo(int origin, int height) : origin(origin), height(height) {}

    // Origin of the row.
    int origin;

    // Height of the row.
    int height;
  };

  // Used to determine row boundaries.
  class Controller {
   public:
    // Returns the origin and size of the row at the specified location.
    virtual VariableRowHeightScrollHelper::RowInfo GetRowInfo(int y) = 0;
  };

  // Creates a new VariableRowHeightScrollHelper. Controller is
  // NOT deleted by this VariableRowHeightScrollHelper.
  explicit VariableRowHeightScrollHelper(Controller* controller);
  virtual ~VariableRowHeightScrollHelper();

  // Delegate the View methods of the same name to these. The scroll amount is
  // determined by querying the Controller for the appropriate row to scroll
  // to.
  int GetPageScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal, bool is_positive);
  int GetLineScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal, bool is_positive);

 protected:
  // Returns the row information for the row at the specified location. This
  // calls through to the method of the same name on the controller.
  virtual RowInfo GetRowInfo(int y);

 private:
  Controller* controller_;

  DISALLOW_EVIL_CONSTRUCTORS(VariableRowHeightScrollHelper);
};

// FixedRowHeightScrollHelper is intended for views that contain fixed height
// height rows. To use a FixedRowHeightScrollHelper delegate
// GetPageScrollIncrement and GetLineScrollIncrement to it.
class FixedRowHeightScrollHelper : public VariableRowHeightScrollHelper {
 public:
  // Creates a FixedRowHeightScrollHelper. top_margin gives the distance from
  // the top of the view to the first row, and may be 0. row_height gives the
  // height of each row.
  FixedRowHeightScrollHelper(int top_margin, int row_height);

 protected:
  // Calculates the bounds of the row from the top margin and row height.
  virtual RowInfo GetRowInfo(int y);

 private:
  int top_margin_;
  int row_height_;

  DISALLOW_EVIL_CONSTRUCTORS(FixedRowHeightScrollHelper);
};

}  // namespace views

#endif // CHROME_VIEWS_SCROLL_VIEW_H__

