// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_2_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_2_H_

#include <vector>

#include "base/task.h"
#include "chrome/browser/views/tabs/tab_2.h"
#include "chrome/browser/views/tabs/tab_strip_wrapper.h"
#include "views/animator.h"
#include "views/view.h"

namespace gfx {
class Canvas;
}

// An interface implemented by an object that provides state for objects in the
// TabStrip2. This object is never owned by the TabStrip2.
// TODO(beng): maybe TabStrip2Delegate?
class TabStrip2Model {
 public:
  // Get presentation state for a particular Tab2.
  virtual string16 GetTitle(int index) const = 0;
  virtual bool IsSelected(int index) const = 0;

  // The Tab2 at the specified index has been selected.
  virtual void SelectTabAt(int index) = 0;

  // Returns true if Tab2s can be dragged.
  virtual bool CanDragTabs() const = 0;

  // The Tab2 at the specified source index has moved to the specified
  // destination index.
  virtual void MoveTabAt(int index, int to_index) = 0;

  // The Tab2 at the specified index was detached. |window_bounds| are the
  // screen bounds of the current window, and |tab_bounds| are the bounds of the
  // Tab2 in screen coordinates.
  virtual void DetachTabAt(int index,
                           const gfx::Rect& window_bounds,
                           const gfx::Rect& tab_bounds) = 0;
};

// A TabStrip view.
class TabStrip2 : public views::View,
                  public Tab2Model,
                  public views::AnimatorDelegate {
 public:
  explicit TabStrip2(TabStrip2Model* model);
  virtual ~TabStrip2();

  // Returns true if the new TabStrip is enabled.
  static bool Enabled();

  // API for adding, removing, selecting and moving tabs around.
  void AddTabAt(int index);
  void RemoveTabAt(int index, Tab2Model* removing_model);
  void SelectTabAt(int index);
  void MoveTabAt(int index, int to_index);

  int GetTabCount() const;
  Tab2* GetTabAt(int index) const;
  int GetTabIndex(Tab2* tab) const;

  // Returns the index to insert an item into the TabStrip at for a drop at the
  // specified point in TabStrip coordinates.
  int GetInsertionIndexForPoint(const gfx::Point& point) const;

  // Returns the bounds of the Tab2 under |screen_point| in screen coordinates.
  gfx::Rect GetDraggedTabScreenBounds(const gfx::Point& screen_point);

  // Sets the bounds of the Tab2 at the specified index to |tab_bounds|.
  void SetDraggedTabBounds(int index, const gfx::Rect& tab_bounds);

  // Animates the dragged Tab2 to the location implied by its index in the
  // model.
  void SendDraggedTabHome();

  // Continue a drag operation on the Tab2 at the specified index.
  void ResumeDraggingTab(int index, const gfx::Rect& tab_bounds);

  // Returns true if the mouse pointer at the specified point (screen bounds)
  // constitutes a rearrange rather than a detach.
  static bool IsDragRearrange(TabStrip2* tabstrip,
                              const gfx::Point& screen_point);

  // Overridden from Tab2Model:
  virtual string16 GetTitle(Tab2* tab) const;
  virtual bool IsSelected(Tab2* tab) const;
  virtual void SelectTab(Tab2* tab);
  virtual void CaptureDragInfo(Tab2* tab, const views::MouseEvent& drag_event);
  virtual bool DragTab(Tab2* tab, const views::MouseEvent& drag_event);
  virtual void DragEnded(Tab2* tab);
  virtual views::AnimatorDelegate* AsAnimatorDelegate();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 private:
  virtual void PaintChildren(gfx::Canvas* canvas);

  // Overridden from views::AnimatorDelegate:
  virtual views::View* GetClampedView(views::View* host);
  virtual void AnimationCompletedForHost(View* host);

  // Specifies what kind of TabStrip2 operation initiated the Layout, so the
  // heuristic can adapt accordingly.
  enum LayoutSource {
    LS_TAB_ADD,
    LS_TAB_REMOVE,
    LS_TAB_SELECT,
    LS_TAB_DRAG_REORDER,
    LS_TAB_DRAG_NORMALIZE,
    LS_OTHER
  };

  // Returns the animation directions for the specified layout source event.
  int GetAnimateFlagsForLayoutSource(LayoutSource source) const;

  // Lays out the contents of the TabStrip2.
  void LayoutImpl(LayoutSource source);

  // Execute the tab detach operation after a return to the message loop.
  void DragDetachTabImpl(Tab2* tab, int index);

  // Execute the drag initiation operation after a return to the message loop.
  void StartDragTabImpl(int index, const gfx::Rect& tab_bounds);

  // Returns the index into |tabs_| that corresponds to a publicly visible
  // index. The index spaces are different since when a tab is closed we retain
  // the tab in the presentation (and this our tab vector) until the tab has
  // animated itself out of existence, but the clients of our API expect that
  // index to be synchronously removed.
  int GetInternalIndex(int public_index) const;

  TabStrip2Model* model_;

  // A vector of our Tabs. Stored separately from the child views, the child
  // view order does not map directly to the presentation order, and because
  // we can have child views that aren't Tab2s.
  std::vector<Tab2*> tabs_;

  // The position of the mouse relative to the widget when drag information was
  // captured.
  gfx::Point mouse_tab_offset_;

  // The last position of the mouse along the horizontal axis of the TabStrip
  // prior to the current drag event. Used to determine that the mouse has moved
  // beyond the minimum horizontal threshold to initiate a drag operation.
  int last_move_screen_x_;

  // Factories to help break up work and avoid nesting message loops.
  ScopedRunnableMethodFactory<TabStrip2> detach_factory_;
  ScopedRunnableMethodFactory<TabStrip2> drag_start_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip2);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_2_H_
