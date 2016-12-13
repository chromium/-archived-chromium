// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_LAYOUT_MANAGER_H_
#define VIEWS_LAYOUT_MANAGER_H_

#include "views/view.h"

namespace gfx {
class Size;
}

namespace views {

class View;

/////////////////////////////////////////////////////////////////////////////
//
// LayoutManager interface
//
//   The LayoutManager interface provides methods to handle the sizing of
//   the children of a View according to implementation-specific heuristics.
//
/////////////////////////////////////////////////////////////////////////////
class LayoutManager {
 public:
  virtual ~LayoutManager() {}

  // Notification that this LayoutManager has been installed on a particular
  // host.
  virtual void Installed(View* host) {}

  // Notification that this LayoutManager has been uninstalled on a particular
  // host.
  virtual void Uninstalled(View* host) {}

  // Lay out the children of |host| according to implementation-specific
  // heuristics. The graphics used during painting is provided to allow for
  // string sizing.
  virtual void Layout(View* host) = 0;

  // Return the preferred size which is the size required to give each
  // children their respective preferred size.
  virtual gfx::Size GetPreferredSize(View* host) = 0;

  // Returns the preferred height for the specified width. The default
  // implementation returns the value from GetPreferredSize.
  virtual int GetPreferredHeightForWidth(View* host, int width);

  // Notification that a view has been added.
  virtual void ViewAdded(View* host, View* view) {}

  // Notification that a view has been removed.
  virtual void ViewRemoved(View* host, View* view) {}
};

}  // namespace views

#endif // VIEWS_LAYOUT_MANAGER_H_
