// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_ROOT_VIEW_DROP_TARGET_H_
#define CHROME_VIEWS_WIDGET_ROOT_VIEW_DROP_TARGET_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "base/base_drop_target.h"
#include "chrome/common/os_exchange_data.h"

namespace gfx {
class Point;
}

namespace views {

class RootView;
class View;

// RootViewDropTarget takes care of managing drag and drop for the RootView and
// converts Windows OLE drop messages into Views drop messages.
//
// RootViewDropTarget is responsible for determining the appropriate View to
// use during a drag and drop session, and forwarding events to it.
class RootViewDropTarget : public BaseDropTarget {
 public:
  explicit RootViewDropTarget(RootView* root_view);
  virtual ~RootViewDropTarget();

  // If a drag and drop is underway and view is the current drop target, the
  // drop target is set to null.
  // This is invoked when a View is removed from the RootView to make sure
  // we don't target a view that was removed during dnd.
  void ResetTargetViewIfEquals(View* view);

 protected:
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  virtual void OnDragLeave(IDataObject* data_object);

  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Calculates the target view for a drop given the specified location in
  // the coordinate system of the rootview. This tries to avoid continually
  // querying CanDrop by returning target_view_ if the mouse is still over
  // target_view_.
  View* CalculateTargetView(const gfx::Point& root_view_location,
                            const OSExchangeData& data);

  // RootView we were created for.
  RootView* root_view_;

  // View we're targeting events at.
  View* target_view_;

  // The deepest view under the current drop coordinate.
  View* deepest_view_;

  DISALLOW_EVIL_CONSTRUCTORS(RootViewDropTarget);
};

}  // namespace views

#endif  // CHROME_VIEWS_WIDGET_ROOT_VIEW_DROP_TARGET_H_
