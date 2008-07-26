// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_ROOT_VIEW_DROP_TARGET_H__
#define CHROME_VIEWS_ROOT_VIEW_DROP_TARGET_H__

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "base/base_drop_target.h"
#include "chrome/common/os_exchange_data.h"

namespace ChromeViews {

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
  View* CalculateTargetView(const CPoint& root_view_location,
                            const OSExchangeData& data);

  // RootView we were created for.
  RootView* root_view_;

  // View we're targeting events at.
  View* target_view_;

  // The deepest view under the current drop coordinate.
  View* deepest_view_;

  DISALLOW_EVIL_CONSTRUCTORS(RootViewDropTarget);
};

}  // namespace

#endif  // CHROME_VIEWS_ROOT_VIEW_DROP_TARGET_H__
