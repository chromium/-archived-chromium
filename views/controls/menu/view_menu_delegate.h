// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_VIEW_MENU_DELEGATE_H_
#define VIEWS_CONTROLS_MENU_VIEW_MENU_DELEGATE_H_

#include "base/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace views {

class View;

////////////////////////////////////////////////////////////////////////////////
//
// ViewMenuDelegate
//
// An interface that allows a component to tell a View about a menu that it
// has constructed that the view can show (e.g. for MenuButton views, or as a
// context menu.)
//
////////////////////////////////////////////////////////////////////////////////
class ViewMenuDelegate {
 public:
  // Create and show a menu at the specified position. Source is the view the
  // ViewMenuDelegate was set on.
  virtual void RunMenu(View* source,
                       const gfx::Point& pt,
                       gfx::NativeView hwnd) = 0;
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_VIEW_MENU_DELEGATE_H_
