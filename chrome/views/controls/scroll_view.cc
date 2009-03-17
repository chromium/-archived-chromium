// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/scroll_view.h"

#include "base/logging.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/controls/scrollbar/native_scroll_bar.h"
#include "chrome/views/widget/root_view.h"
#include "grit/theme_resources.h"

namespace views {

const char* const ScrollView::kViewClassName = "chrome/views/ScrollView";

// Viewport contains the contents View of the ScrollView.
class Viewport : public View {
 public:
  Viewport() {}
  virtual ~Viewport() {}

  virtual void ScrollRectToVisible(int x, int y, int width, int height) {
    if (!GetChildViewCount() || !GetParent())
      return;

    View* contents = GetChildViewAt(0);
    x -= contents->x();
    y -= contents->y();
    static_cast<ScrollView*>(GetParent())->ScrollContentsRegionToBeVisible(
        x, y, width, height);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Viewport);
};


ScrollView::ScrollView() {
  Init(new NativeScrollBar(true), new NativeScrollBar(false), NULL);
}

ScrollView::ScrollView(ScrollBar* horizontal_scrollbar,
                       ScrollBar* vertical_scrollbar,
                       View* resize_corner) {
  Init(horizontal_scrollbar, vertical_scrollbar, resize_corner);
}

ScrollView::~ScrollView() {
  // If scrollbars are currently not used, delete them
  if (!horiz_sb_->GetParent()) {
    delete horiz_sb_;
  }

  if (!vert_sb_->GetParent()) {
    delete vert_sb_;
  }

  if (resize_corner_ && !resize_corner_->GetParent()) {
    delete resize_corner_;
  }
}

void ScrollView::SetContents(View* a_view) {
  if (contents_ && contents_ != a_view) {
    viewport_->RemoveChildView(contents_);
    delete contents_;
    contents_ = NULL;
  }

  if (a_view) {
    contents_ = a_view;
    viewport_->AddChildView(contents_);
  }

  Layout();
}

View* ScrollView::GetContents() const {
  return contents_;
}

void ScrollView::Init(ScrollBar* horizontal_scrollbar,
                      ScrollBar* vertical_scrollbar,
                      View* resize_corner) {
  DCHECK(horizontal_scrollbar && vertical_scrollbar);

  contents_ = NULL;
  horiz_sb_ = horizontal_scrollbar;
  vert_sb_ = vertical_scrollbar;
  resize_corner_ = resize_corner;

  viewport_ = new Viewport();
  AddChildView(viewport_);

  // Don't add the scrollbars as children until we discover we need them
  // (ShowOrHideScrollBar).
  horiz_sb_->SetVisible(false);
  horiz_sb_->SetController(this);
  vert_sb_->SetVisible(false);
  vert_sb_->SetController(this);
  if (resize_corner_)
    resize_corner_->SetVisible(false);
}

// Make sure that a single scrollbar is created and visible as needed
void ScrollView::SetControlVisibility(View* control, bool should_show) {
  if (!control)
    return;
  if (should_show) {
    if (!control->IsVisible()) {
      AddChildView(control);
      control->SetVisible(true);
    }
  } else {
    RemoveChildView(control);
    control->SetVisible(false);
  }
}

void ScrollView::ComputeScrollBarsVisibility(const gfx::Size& vp_size,
                                             const gfx::Size& content_size,
                                             bool* horiz_is_shown,
                                             bool* vert_is_shown) const {
  // Try to fit both ways first, then try vertical bar only, then horizontal
  // bar only, then defaults to both shown.
  if (content_size.width() <= vp_size.width() &&
      content_size.height() <= vp_size.height()) {
    *horiz_is_shown = false;
    *vert_is_shown = false;
  } else if (content_size.width() <= vp_size.width() - GetScrollBarWidth()) {
    *horiz_is_shown = false;
    *vert_is_shown = true;
  } else if (content_size.height() <= vp_size.height() - GetScrollBarHeight()) {
    *horiz_is_shown = true;
    *vert_is_shown = false;
  } else {
    *horiz_is_shown = true;
    *vert_is_shown = true;
  }
}

void ScrollView::Layout() {
  // Most views will want to auto-fit the available space. Most of them want to
  // use the all available width (without overflowing) and only overflow in
  // height. Examples are HistoryView, MostVisitedView, DownloadTabView, etc.
  // Other views want to fit in both ways. An example is PrintView. To make both
  // happy, assume a vertical scrollbar but no horizontal scrollbar. To
  // override this default behavior, the inner view has to calculate the
  // available space, used ComputeScrollBarsVisibility() to use the same
  // calculation that is done here and sets its bound to fit within.
  gfx::Rect viewport_bounds = GetLocalBounds(true);
  // Realign it to 0 so it can be used as-is for SetBounds().
  viewport_bounds.set_origin(gfx::Point(0, 0));
  // viewport_size is the total client space available.
  gfx::Size viewport_size = viewport_bounds.size();
  if (viewport_bounds.IsEmpty()) {
    // There's nothing to layout.
    return;
  }

  // Assumes a vertical scrollbar since most the current views are designed for
  // this.
  int horiz_sb_height = GetScrollBarHeight();
  int vert_sb_width = GetScrollBarWidth();
  viewport_bounds.set_width(viewport_bounds.width() - vert_sb_width);
  // Update the bounds right now so the inner views can fit in it.
  viewport_->SetBounds(viewport_bounds);

  // Give contents_ a chance to update its bounds if it depends on the
  // viewport.
  if (contents_)
    contents_->Layout();

  bool should_layout_contents = false;
  bool horiz_sb_required = false;
  bool vert_sb_required = false;
  if (contents_) {
    gfx::Size content_size = contents_->size();
    ComputeScrollBarsVisibility(viewport_size,
                                content_size,
                                &horiz_sb_required,
                                &vert_sb_required);
  }
  bool resize_corner_required = resize_corner_ && horiz_sb_required &&
                                vert_sb_required;
  // Take action.
  SetControlVisibility(horiz_sb_, horiz_sb_required);
  SetControlVisibility(vert_sb_, vert_sb_required);
  SetControlVisibility(resize_corner_, resize_corner_required);

  // Non-default.
  if (horiz_sb_required) {
    viewport_bounds.set_height(viewport_bounds.height() - horiz_sb_height);
    should_layout_contents = true;
  }
  // Default.
  if (!vert_sb_required) {
    viewport_bounds.set_width(viewport_bounds.width() + vert_sb_width);
    should_layout_contents = true;
  }

  if (horiz_sb_required) {
    horiz_sb_->SetBounds(0,
                         viewport_bounds.bottom(),
                         viewport_bounds.right(),
                         horiz_sb_height);
  }
  if (vert_sb_required) {
    vert_sb_->SetBounds(viewport_bounds.right(),
                        0,
                        vert_sb_width,
                        viewport_bounds.bottom());
  }
  if (resize_corner_required) {
    // Show the resize corner.
    resize_corner_->SetBounds(viewport_bounds.right(),
                              viewport_bounds.bottom(),
                              vert_sb_width,
                              horiz_sb_height);
  }

  // Update to the real client size with the visible scrollbars.
  viewport_->SetBounds(viewport_bounds);
  if (should_layout_contents && contents_)
    contents_->Layout();

  CheckScrollBounds();
  SchedulePaint();
  UpdateScrollBarPositions();
}

int ScrollView::CheckScrollBounds(int viewport_size,
                                  int content_size,
                                  int current_pos) {
  int max = std::max(content_size - viewport_size, 0);
  if (current_pos < 0)
    current_pos = 0;
  else if (current_pos > max)
    current_pos = max;
  return current_pos;
}

void ScrollView::CheckScrollBounds() {
  if (contents_) {
    int x, y;

    x = CheckScrollBounds(viewport_->width(),
                          contents_->width(),
                          -contents_->x());
    y = CheckScrollBounds(viewport_->height(),
                          contents_->height(),
                          -contents_->y());

    // This is no op if bounds are the same
    contents_->SetBounds(-x, -y, contents_->width(), contents_->height());
  }
}

gfx::Rect ScrollView::GetVisibleRect() const {
  if (!contents_)
    return gfx::Rect();

  const int x =
      (horiz_sb_ && horiz_sb_->IsVisible()) ? horiz_sb_->GetPosition() : 0;
  const int y =
      (vert_sb_ && vert_sb_->IsVisible()) ? vert_sb_->GetPosition() : 0;
  return gfx::Rect(x, y, viewport_->width(), viewport_->height());
}

void ScrollView::ScrollContentsRegionToBeVisible(int x,
                                                 int y,
                                                 int width,
                                                 int height) {
  if (!contents_ || ((!horiz_sb_ || !horiz_sb_->IsVisible()) &&
                     (!vert_sb_ || !vert_sb_->IsVisible()))) {
    return;
  }

  const int contents_max_x =
      std::max(viewport_->width(), contents_->width());
  const int contents_max_y =
      std::max(viewport_->height(), contents_->height());
  x = std::max(0, std::min(contents_max_x, x));
  y = std::max(0, std::min(contents_max_y, y));
  const int max_x = std::min(contents_max_x,
                             x + std::min(width, viewport_->width()));
  const int max_y = std::min(contents_max_y,
                             y + std::min(height,
                                          viewport_->height()));
  const gfx::Rect vis_rect = GetVisibleRect();
  if (vis_rect.Contains(gfx::Rect(x, y, max_x, max_y)))
    return;

  const int new_x =
      (vis_rect.x() < x) ? x : std::max(0, max_x - viewport_->width());
  const int new_y =
      (vis_rect.y() < y) ? y : std::max(0, max_x - viewport_->height());

  contents_->SetX(-new_x);
  contents_->SetY(-new_y);
  UpdateScrollBarPositions();
}

void ScrollView::UpdateScrollBarPositions() {
  if (!contents_) {
    return;
  }

  if (horiz_sb_->IsVisible()) {
    int vw = viewport_->width();
    int cw = contents_->width();
    int origin = contents_->x();
    horiz_sb_->Update(vw, cw, -origin);
  }
  if (vert_sb_->IsVisible()) {
    int vh = viewport_->height();
    int ch = contents_->height();
    int origin = contents_->y();
    vert_sb_->Update(vh, ch, -origin);
  }
}

// TODO(ACW). We should really use ScrollWindowEx as needed
void ScrollView::ScrollToPosition(ScrollBar* source, int position) {
  if (!contents_)
    return;

  if (source == horiz_sb_ && horiz_sb_->IsVisible()) {
    int vw = viewport_->width();
    int cw = contents_->width();
    int origin = contents_->x();
    if (-origin != position) {
      int max_pos = std::max(0, cw - vw);
      if (position < 0)
        position = 0;
      else if (position > max_pos)
        position = max_pos;
      contents_->SetX(-position);
      contents_->SchedulePaint(contents_->GetLocalBounds(true), true);
    }
  } else if (source == vert_sb_ && vert_sb_->IsVisible()) {
    int vh = viewport_->height();
    int ch = contents_->height();
    int origin = contents_->y();
    if (-origin != position) {
      int max_pos = std::max(0, ch - vh);
      if (position < 0)
        position = 0;
      else if (position > max_pos)
        position = max_pos;
      contents_->SetY(-position);
      contents_->SchedulePaint(contents_->GetLocalBounds(true), true);
    }
  }
}

int ScrollView::GetScrollIncrement(ScrollBar* source, bool is_page,
                                   bool is_positive) {
  bool is_horizontal = source->IsHorizontal();
  int amount = 0;
  View* view = GetContents();
  if (view) {
    if (is_page)
      amount = view->GetPageScrollIncrement(this, is_horizontal, is_positive);
    else
      amount = view->GetLineScrollIncrement(this, is_horizontal, is_positive);
    if (amount > 0)
      return amount;
  }
  // No view, or the view didn't return a valid amount.
  if (is_page)
    return is_horizontal ? viewport_->width() : viewport_->height();
  return is_horizontal ? viewport_->width() / 5 : viewport_->height() / 5;
}

void ScrollView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add) {
    RootView* rv = GetRootView();
    if (rv) {
      rv->SetDefaultKeyboardHandler(this);
      rv->SetFocusOnMousePressed(true);
    }
  }
}

bool ScrollView::OnKeyPressed(const KeyEvent& event) {
  bool processed = false;

  // Give vertical scrollbar priority
  if (vert_sb_->IsVisible()) {
    processed = vert_sb_->OnKeyPressed(event);
  }

  if (!processed && horiz_sb_->IsVisible()) {
    processed = horiz_sb_->OnKeyPressed(event);
  }
  return processed;
}

bool ScrollView::OnMouseWheel(const MouseWheelEvent& e) {
  bool processed = false;

  // Give vertical scrollbar priority
  if (vert_sb_->IsVisible()) {
    processed = vert_sb_->OnMouseWheel(e);
  }

  if (!processed && horiz_sb_->IsVisible()) {
    processed = horiz_sb_->OnMouseWheel(e);
  }
  return processed;
}

std::string ScrollView::GetClassName() const {
  return kViewClassName;
}

int ScrollView::GetScrollBarWidth() const {
  return vert_sb_->GetLayoutSize();
}

int ScrollView::GetScrollBarHeight() const {
  return horiz_sb_->GetLayoutSize();
}

// VariableRowHeightScrollHelper ----------------------------------------------

VariableRowHeightScrollHelper::VariableRowHeightScrollHelper(
    Controller* controller) : controller_(controller) {
}

VariableRowHeightScrollHelper::~VariableRowHeightScrollHelper() {
}

int VariableRowHeightScrollHelper::GetPageScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->GetContents()->y());
  int vis_height = scroll_view->GetContents()->GetParent()->height();
  if (is_positive) {
    // Align the bottom most row to the top of the view.
    int bottom = std::min(scroll_view->GetContents()->height() - 1,
                          y + vis_height);
    RowInfo bottom_row_info = GetRowInfo(bottom);
    // If 0, ScrollView will provide a default value.
    return std::max(0, bottom_row_info.origin - y);
  } else {
    // Align the row on the previous page to to the top of the view.
    int last_page_y = y - vis_height;
    RowInfo last_page_info = GetRowInfo(std::max(0, last_page_y));
    if (last_page_y != last_page_info.origin)
      return std::max(0, y - last_page_info.origin - last_page_info.height);
    return std::max(0, y - last_page_info.origin);
  }
}

int VariableRowHeightScrollHelper::GetLineScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->GetContents()->y());
  RowInfo row = GetRowInfo(y);
  if (is_positive) {
    return row.height - (y - row.origin);
  } else if (y == row.origin) {
    row = GetRowInfo(std::max(0, row.origin - 1));
    return y - row.origin;
  } else {
    return y - row.origin;
  }
}

VariableRowHeightScrollHelper::RowInfo
    VariableRowHeightScrollHelper::GetRowInfo(int y) {
  return controller_->GetRowInfo(y);
}

// FixedRowHeightScrollHelper -----------------------------------------------

FixedRowHeightScrollHelper::FixedRowHeightScrollHelper(int top_margin,
                                                       int row_height)
    : VariableRowHeightScrollHelper(NULL),
      top_margin_(top_margin),
      row_height_(row_height) {
  DCHECK(row_height > 0);
}

VariableRowHeightScrollHelper::RowInfo
    FixedRowHeightScrollHelper::GetRowInfo(int y) {
  if (y < top_margin_)
    return RowInfo(0, top_margin_);
  return RowInfo((y - top_margin_) / row_height_ * row_height_ + top_margin_,
                 row_height_);
}

}  // namespace views
