// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_BUTTON_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_BUTTON_GTK_H_

#include <gdk/gdk.h>

#include "base/gfx/rect.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "skia/include/SkBitmap.h"

///////////////////////////////////////////////////////////////////////////////
// TabButtonGtk
//
//  Performs hit-testing and rendering of a Tab button.
//
class TabButtonGtk {
 public:
  // Possible button states
  enum ButtonState {
    BS_NORMAL,
    BS_HOT,
    BS_PUSHED,
    BS_COUNT
  };

  class Delegate {
   public:
    // Creates a clickable region of the button's visual representation used
    // for hit-testing.  Caller is responsible for destroying the region.  If
    // NULL is returned, the bounds of the button will be used for hit-testing.
    virtual GdkRegion* MakeRegionForButton(
        const TabButtonGtk* button) const = 0;

    // Sent when the user activates the button, which is defined as a press
    // and release of a mouse click over the button.
    virtual void OnButtonActivate(const TabButtonGtk* button) = 0;
  };

  explicit TabButtonGtk(Delegate* delegate);
  virtual ~TabButtonGtk();

  // Returns the bounds of the button.
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }

  const gfx::Rect& bounds() const { return bounds_; }

  // Sets the bounds of the button.
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // Checks whether |point| is inside the bounds of the button.
  bool IsPointInBounds(const gfx::Point& point);

  // Sent by the tabstrip when the mouse moves within this button.  Mouse state
  // is in |event|.  Returns true if the tabstrip needs to be redrawn as a
  // result of the motion.
  bool OnMotionNotify(GdkEventMotion* event);

  // Sent by the tabstrip when the mouse clicks within this button.  Returns
  // true if the tabstrip needs to be redrawn as a result of the click.
  bool OnMousePress();

  // Sent by the tabstrip when the mouse click is released.
  void OnMouseRelease();

  // Sent by the tabstrip when the mouse leaves this button.  Returns true
  // if the tabstrip needs to be redrawn as a result of the movement.
  bool OnLeaveNotify();

  // Paints the Tab button into |canvas|.
  void Paint(ChromeCanvasPaint* canvas);

  // Sets the image the button should use for the provided state.
  void SetImage(ButtonState state, SkBitmap* bitmap);

 private:
  // When the tab animation completes, we send the widget a message to
  // simulate a mouse moved event at the current mouse position. This tickles
  // the button to show the "hot" state.
  void HighlightTabButton();

  // The images used to render the different states of this button.
  SkBitmap images_[BS_COUNT];

  // The current state of the button.
  ButtonState state_;

  // The current bounds of the button.
  gfx::Rect bounds_;

  // Set if the mouse is pressed anywhere inside the button.
  bool mouse_pressed_;

  // Delegate to receive button messages.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabButtonGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_BUTTON_GTK_H_
