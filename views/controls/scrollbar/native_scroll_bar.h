// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLLBAR_H_
#define VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLLBAR_H_

#include "build/build_config.h"

#include "views/controls/scrollbar/scroll_bar.h"

namespace views {

class NativeViewHost;
class ScrollBarContainer;

/////////////////////////////////////////////////////////////////////////////
//
// NativeScrollBar
//
// A View subclass that wraps a Native Windows scrollbar control.
//
// A scrollbar is either horizontal or vertical.
//
/////////////////////////////////////////////////////////////////////////////
class NativeScrollBar : public ScrollBar {
 public:

  // Create new scrollbar, either horizontal or vertical
  explicit NativeScrollBar(bool is_horiz);
  virtual ~NativeScrollBar();

  // Overridden for layout purpose
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Overridden for keyboard UI purpose
  virtual bool OnKeyPressed(const KeyEvent& event);
  virtual bool OnMouseWheel(const MouseWheelEvent& e);

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from ScrollBar
  virtual void Update(int viewport_size, int content_size, int current_pos);
  virtual int GetLayoutSize() const;
  virtual int GetPosition() const;

  // Return the system sizes
  static int GetHorizontalScrollBarHeight();
  static int GetVerticalScrollBarWidth();

 private:
#if defined(OS_WIN)
  // The sb_view_ takes care of keeping sb_container in sync with the
  // view hierarchy
  NativeViewHost* sb_view_;
#endif  // defined(OS_WIN)

  // sb_container_ is a custom hwnd that we use to wrap the real
  // windows scrollbar. We need to do this to get the scroll events
  // without having to do anything special in the high level hwnd.
  ScrollBarContainer* sb_container_;
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLLBAR_H_
