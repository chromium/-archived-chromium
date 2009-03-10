// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_VIEW_MENU_DELEGATE_H__
#define CHROME_VIEWS_VIEW_MENU_DELEGATE_H__

#include <windows.h>

namespace views {

class View;

////////////////////////////////////////////////////////////////////////////////
//
// MenuDelegate
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
  virtual void RunMenu(View* source, const CPoint& pt, HWND hwnd) = 0;
};

}  // namespace views

#endif  // CHROME_VIEWS_VIEW_MENU_DELEGATE_H__
