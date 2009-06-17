// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#define CHROME_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class ConstrainedWindow;
namespace views {
class View;
class WindowDelegate;
}
namespace gfx {
class Point;
class Rect;
}
class GURL;
class TabContents;

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a TabContents'
//  bounds.
//
class ConstrainedWindow {
 public:
  // Create a Constrained Window that contains a views::View subclass
  // that provides the client area. Typical uses include the HTTP Basic Auth
  // prompt. The caller must provide an object implementing
  // views::WindowDelegate so that the Constrained Window can be properly
  // configured. If |initial_bounds| is empty, the dialog will be centered
  // within the constraining TabContents.
  static ConstrainedWindow* CreateConstrainedDialog(
      TabContents* owner,
      views::WindowDelegate* window_delegate);

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;
};

#endif  // #ifndef CHROME_TAB_CONTENTS_BROWSER_CONSTRAINED_WINDOW_H_
