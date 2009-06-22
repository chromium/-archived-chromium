// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/grid.h"

#include "base/compiler_specific.h"

using views::View;

//static
const int Grid::kCellXPadding = 15;
// static
const int Grid::kCellYPadding = 15;

Grid::Grid()
    : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)),
      modifying_children_(false),
      needs_layout_(false),
      pref_width_(0),
      pref_height_(0),
      cell_width_(0),
      cell_height_(0),
      columns_(0),
      rows_(0),
      floating_index_(-1) {
  animation_.SetTweenType(SlideAnimation::EASE_OUT);
}

void Grid::MoveCell(int old_index, int new_index) {
  View* cell = GetChildViewAt(old_index);
  modifying_children_ = true;
  RemoveChildView(cell);
  AddChildView(new_index, cell);
  modifying_children_ = false;

  CalculateTargetBoundsAndStartAnimation();
}

void Grid::InsertCell(int index, View* cell) {
  modifying_children_ = true;
  AddChildView(index, cell);
  modifying_children_ = false;

  CalculateTargetBoundsAndStartAnimation();

  // Set the bounds of the cell to it's target bounds. This way it won't appear
  // to animate.
  if (index != floating_index_) {
    start_bounds_[index] = target_bounds_[index];
    cell->SetBounds(target_bounds_[index]);
  }
}

void Grid::RemoveCell(int index) {
  modifying_children_ = true;
  RemoveChildView(GetChildViewAt(index));
  modifying_children_ = false;

  CalculateTargetBoundsAndStartAnimation();
}

void Grid::AnimateToTargetBounds() {
  CalculateTargetBoundsAndStartAnimation();
}

int Grid::AnimationPosition(int start, int target) {
  return start + static_cast<int>(
      static_cast<double>(target - start) * animation_.GetCurrentValue());
}

gfx::Rect Grid::AnimationPosition(const gfx::Rect& start_bounds,
                                  const gfx::Rect& target_bounds) {
  return gfx::Rect(AnimationPosition(start_bounds.x(), target_bounds.x()),
                   AnimationPosition(start_bounds.y(), target_bounds.y()),
                   AnimationPosition(start_bounds.width(),
                                     target_bounds.width()),
                   AnimationPosition(start_bounds.height(),
                                     target_bounds.height()));
}

void Grid::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (modifying_children_ || parent != this)
    return;

  // Our child views changed without us knowing it. Stop the animation and mark
  // us as dirty (needs_layout_ = true).
  animation_.Stop();
  needs_layout_ = true;
}

gfx::Rect Grid::CellBounds(int index) {
  int row = index / columns_;
  int col = index % columns_;
  return gfx::Rect(col * cell_width_ + std::max(0, col * kCellXPadding),
                   row * cell_height_ + std::max(0, row * kCellYPadding),
                   cell_width_, cell_height_);
}

gfx::Size Grid::GetPreferredSize() {
  if (needs_layout_)
    Layout();

  return gfx::Size(pref_width_, pref_height_);
}

void Grid::Layout() {
  if (!needs_layout_)
    return;

  needs_layout_ = false;
  animation_.Stop();
  target_bounds_.clear();
  CalculateCellBounds(&target_bounds_);
  for (size_t i = 0; i < target_bounds_.size(); ++i) {
    if (static_cast<int>(i) != floating_index_)
      GetChildViewAt(i)->SetBounds(target_bounds_[i]);
  }
}

void Grid::PaintChildren(gfx::Canvas* canvas) {
  int i, c;
  for (i = 0, c = GetChildViewCount(); i < c; ++i) {
    if (i != floating_index_) {
      View* child = GetChildViewAt(i);
      if (!child) {
        NOTREACHED();
        continue;
      }
      child->ProcessPaint(canvas);
    }
  }

  // Paint the floating view last. This way it floats on top of all other
  // views.
  if (floating_index_ != -1 && floating_index_ < GetChildViewCount())
    GetChildViewAt(floating_index_)->ProcessPaint(canvas);
}

void Grid::AnimationEnded(const Animation* animation) {
  SetViewBoundsToTarget();
}

void Grid::AnimationProgressed(const Animation* animation) {
  DCHECK(GetChildViewCount() == static_cast<int>(target_bounds_.size()));
  for (size_t i = 0; i < target_bounds_.size(); ++i) {
    View* view = GetChildViewAt(i);
    gfx::Rect start_bounds = start_bounds_[i];
    gfx::Rect target_bounds = target_bounds_[i];
    if (static_cast<int>(i) != floating_index_)
      view->SetBounds(AnimationPosition(start_bounds, target_bounds));
  }
  SchedulePaint();
}

void Grid::AnimationCanceled(const Animation* animation) {
  // Don't do anything when the animation is canceled. Presumably Layout will
  // be invoked, and all children will get set to their appropriate position.
}

void Grid::CalculateCellBounds(std::vector<gfx::Rect>* bounds) {
  DCHECK(max_size_.width() > 0 && max_size_.height() > 0);
  int cell_count = GetChildViewCount();
  if (cell_count == 0) {
    pref_width_ = pref_height_ = 0;
    return;
  }

  gfx::Size cell_pref = GetChildViewAt(0)->GetPreferredSize();
  int col_count, row_count;
  // Assume we get the ideal cell size.
  int cell_width = cell_pref.width();
  int cell_height = cell_pref.height();
  int max_columns = std::max(1, (max_size_.width() + kCellXPadding) /
                             (cell_width + kCellXPadding));
  if (cell_count <= max_columns) {
    // All the cells fit in a single row.
    row_count = 1;
    col_count = cell_count;
  } else {
    // Need more than one row to display all.
    int max_rows = std::max(1, (max_size_.height() + kCellYPadding) /
                            (cell_height + kCellYPadding));
    col_count = max_columns;
    row_count = cell_count / max_columns;
    if (cell_count % col_count != 0)
      row_count++;
    if (cell_count > max_columns * max_rows) {
      // We don't have enough space for the cells at their ideal size. Keep
      // adding columns (and shrinking down cell sizes) until we fit
      // everything.
      float ratio = static_cast<float>(cell_width) /
          static_cast<float>(cell_height);
      do {
        col_count++;
        cell_width =
            static_cast<float>(max_size_.width() -
                               ((col_count - 1) * kCellXPadding)) /
            static_cast<float>(col_count);
        cell_height = static_cast<float>(cell_width) / ratio;
        row_count = std::max(1, (max_size_.height() + kCellYPadding) /
                             (cell_height + kCellYPadding));
      } while (row_count * col_count < cell_count);
      row_count = cell_count / col_count;
      if (cell_count % col_count != 0)
        row_count++;
    }
  }

  cell_width_ = cell_width;
  cell_height_ = cell_height;
  columns_ = col_count;
  rows_ = row_count;

  pref_width_ =
      std::max(0, col_count * (cell_width + kCellXPadding) - kCellXPadding);
  pref_height_ =
      std::max(0, row_count * (cell_height + kCellYPadding) - kCellYPadding);

  for (int i = 0; i < cell_count; ++i)
    bounds->push_back(CellBounds(i));
}

void Grid::CalculateTargetBoundsAndStartAnimation() {
  if (needs_layout_)
    Layout();

  // Determine the current bounds.
  start_bounds_.clear();
  start_bounds_.resize(GetChildViewCount());
  for (int i = 0; i < GetChildViewCount(); ++i)
    start_bounds_[i] = GetChildViewAt(i)->bounds();

  // Then the target bounds.
  target_bounds_.clear();
  CalculateCellBounds(&target_bounds_);

  // And restart the animation.
  animation_.Reset();
  animation_.Show();
}

void Grid::SetViewBoundsToTarget() {
  DCHECK(GetChildViewCount() == static_cast<int>(target_bounds_.size()));
  for (size_t i = 0; i < target_bounds_.size(); ++i) {
    if (static_cast<int>(i) != floating_index_)
      GetChildViewAt(i)->SetBounds(target_bounds_[i]);
  }
}
