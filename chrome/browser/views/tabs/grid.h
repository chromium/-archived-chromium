// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_GRID_H_
#define CHROME_BROWSER_VIEWS_TABS_GRID_H_

#include <vector>

#include "app/slide_animation.h"
#include "base/gfx/rect.h"
#include "views/view.h"

// Grid is a view that positions its children (cells) in a grid. Grid
// attempts to layout the children at their preferred size (assuming
// all cells have the same preferred size) in a single row. If the sum
// of the widths is greater than the max width, then a new row is
// added. Once the max number of rows and columns are reached, the
// cells are shrunk to fit.
//
// Grid offers methods to move, insert and remove cells. These end up changing
// the child views, and animating the transition.
class Grid : public views::View, public AnimationDelegate {
 public:
  Grid();

  // Sets the max size for the Grid. See description above class for details.
  void set_max_size(const gfx::Size& size) { max_size_ = size; }
  const gfx::Size&  max_size() const { return max_size_; }

  // Moves the child view to the specified index, animating the move.
  void MoveCell(int old_index, int new_index);

  // Inserts a cell at the specified index, animating the insertion.
  void InsertCell(int index, views::View* cell);

  // Removes the cell at the specified index, animating the removal.
  // WARNING: this does NOT delete the view, it's up to the caller to do that.
  void RemoveCell(int index);

  // Calculates the target bounds of each cell and starts the animation timer
  // (assuming it isn't already running). This is invoked for you, but may
  // be invoked to retrigger animation, perhaps after changing the floating
  // index.
  void AnimateToTargetBounds();

  // Sets the index of the floating cell. The floating cells bounds are NOT
  // updated along with the rest of the cells, and the floating cell is painted
  // after all other cells. This is typically used during drag and drop when
  // the user is dragging a cell around.
  void set_floating_index(int index) { floating_index_ = index; }

  // Returns the number of columns.
  int columns() const { return columns_; }

  // Returns the number of rows.
  int rows() const { return rows_; }

  // Returns the width of a cell.
  int cell_width() const { return cell_width_; }

  // Returns the height of a cell.
  int cell_height() const { return cell_height_; }

  // Returns the bounds of the specified cell.
  gfx::Rect CellBounds(int index);

  // Returns the value based on the current animation. |start| gives the
  // starting coordinate and |target| the target coordinate. The resulting
  // value is between |start| and |target| based on the current animation.
  int AnimationPosition(int start, int target);

  // Convenience for returning a rectangle between |start_bounds| and
  // |target_bounds| based on the current animation.
  gfx::Rect AnimationPosition(const gfx::Rect& start_bounds,
                              const gfx::Rect& target_bounds);

  // View overrides.
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  void PaintChildren(gfx::Canvas* canvas);

  // AnimationDelegate overrides.
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

  // Padding between cells.
  static const int kCellXPadding;
  static const int kCellYPadding;

 private:
  // Calculates the bounds of each of the cells, adding the result to |bounds|.
  void CalculateCellBounds(std::vector<gfx::Rect>* bounds);

  // Resets start_bounds_ to the bounds of the current cells, and invokes
  // CalculateCellBounds to determine the target bounds. Then starts the
  // animation if it isn't already running.
  void CalculateTargetBoundsAndStartAnimation();

  // Resets the bounds of each cell to that of target_bounds_.
  void SetViewBoundsToTarget();

  // The animation.
  SlideAnimation animation_;

  // If true, we're adding/removing a child and can ignore the change in
  // ViewHierarchyChanged.
  bool modifying_children_;

  // Do we need a layout? This is set to true any time a child is added/removed.
  bool needs_layout_;

  // Max size we layout to.
  gfx::Size max_size_;

  // Preferred size.
  int pref_width_;
  int pref_height_;

  // Current cell size.
  int cell_width_;
  int cell_height_;

  // Number of rows/columns.
  int columns_;
  int rows_;

  // See description above setter.
  int floating_index_;

  // Used during animation, gives the initial bounds of the views.
  std::vector<gfx::Rect> start_bounds_;

  // Used during animation, gives the target bounds of the views.
  std::vector<gfx::Rect> target_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Grid);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_GRID_H_
