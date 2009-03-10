// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_DRAGGED_TAB_VIEW_H_
#define CHROME_BROWSER_VIEWS_TABS_DRAGGED_TAB_VIEW_H_

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "base/task.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/view.h"
#include "skia/include/SkBitmap.h"

namespace views {
class WidgetWin;
}
namespace gfx {
class Point;
}
class HWNDPhotobooth;
class Tab;
class TabContents;
class TabRenderer;

class DraggedTabView : public views::View,
                       public AnimationDelegate {
 public:
  DraggedTabView(TabContents* datasource,
                 const gfx::Point& mouse_tab_offset,
                 const gfx::Size& contents_size);
  virtual ~DraggedTabView();

  // Moves the DraggedTabView to the appropriate location given the mouse
  // pointer at |screen_point|.
  void MoveTo(const gfx::Point& screen_point);

  // Notifies the DraggedTabView that it has become attached to a TabStrip.
  void Attach(int selected_width);

  // Notifies the DraggedTabView that it has been detached from a TabStrip.
  void Detach(HWNDPhotobooth* photobooth);

  // Notifies the DraggedTabView that it should update itself.
  void Update();

  // Animates the DraggedTabView to the specified bounds, then calls back to
  // |callback|.
  void AnimateToBounds(const gfx::Rect& bounds, Callback0::Type* callback);

  // Returns the size of the DraggedTabView. Used when attaching to a TabStrip
  // to determine where to place the Tab in the attached TabStrip.
  gfx::Size attached_tab_size() const { return attached_tab_size_; }

 private:
  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Paint the view, when it's attached to a TabStrip.
  void PaintAttachedTab(ChromeCanvas* canvas);

  // Paint the view, when it's not attached to any TabStrip.
  void PaintDetachedView(ChromeCanvas* canvas);

  // Resizes the container to fit the content for the current attachment mode.
  void ResizeContainer();

  // Utility for scaling a size by the current scaling factor.
  int ScaleValue(int value);

  // The window that contains the DraggedTabView.
  scoped_ptr<views::WidgetWin> container_;

  // The renderer that paints the Tab shape.
  scoped_ptr<TabRenderer> renderer_;

  // True if the view is currently attached to a TabStrip. Controls rendering
  // and sizing modes.
  bool attached_;

  // The unscaled offset of the mouse from the top left of the dragged Tab.
  // This is used to maintain an appropriate offset for the mouse pointer when
  // dragging scaled and unscaled representations, and also to calculate the
  // position of detached windows.
  gfx::Point mouse_tab_offset_;

  // The desired width of the TabRenderer when the DraggedTabView is attached
  // to a TabStrip.
  gfx::Size attached_tab_size_;

  // A handle to the DIB containing the current screenshot of the TabContents
  // we are dragging.
  HWNDPhotobooth* photobooth_;

  // The dimensions of the TabContents being dragged.
  gfx::Size contents_size_;

  // The animation used to slide the attached view to its final location.
  SlideAnimation close_animation_;

  // A callback notified when the animation is complete.
  scoped_ptr<Callback0::Type> animation_callback_;

  // The start and end bounds of the animation sequence.
  gfx::Rect animation_start_bounds_;
  gfx::Rect animation_end_bounds_;

  DISALLOW_EVIL_CONSTRUCTORS(DraggedTabView);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_DRAGGED_TAB_VIEW_H_
