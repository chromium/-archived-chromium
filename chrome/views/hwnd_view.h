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

#ifndef CHROME_VIEWS_HWND_VIEW_H__
#define CHROME_VIEWS_HWND_VIEW_H__

#include <string>

#include "chrome/views/view.h"

namespace ChromeViews {

/////////////////////////////////////////////////////////////////////////////
//
// HWNDView class
//
//   The HWNDView class hosts a native window handle (HWND) sizing it
//   according to the bounds of the view. This is useful whenever you need to
//   show a UI control that has a HWND (e.g. a native windows Edit control)
//   within thew View hierarchy and benefit from the sizing/layout.
//
/////////////////////////////////////////////////////////////////////////////
class HWNDView : public View {
 public:
  HWNDView();
  virtual ~HWNDView();

  virtual void GetPreferredSize(CSize *out);

  void SetPreferredSize(const CSize& size);

  // Attach a window handle to this View, making the window it represents
  // subject to sizing according to this View's parent container's Layout
  // Manager's sizing heuristics.
  void Attach(HWND hwnd);

  // Detach the attached window handle. It will no longer be updated
  void Detach();

  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  HWND GetHWND() const;

  // Resize the hosted HWND to the bounds of this View.
  void UpdateHWNDBounds();

  virtual void Paint(ChromeCanvas* canvas);

  // Overridden from View.
  virtual std::string GetClassName() const;

  // A HWNDView has an associated focus View so that the focus of the native
  // control and of the View are kept in sync. In simple cases where the
  // HWNDView directly wraps a native window as is, the associated view is this
  // View. In other cases where the HWNDView is part of another view (such as
  // TextField), the actual View is not the HWNDView and this method must be
  // called to set that.
  // This method must be called before Attach().
  void SetAssociatedFocusView(View* view);

  void set_fast_resize(bool fast_resize) { fast_resize_ = fast_resize; }

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Notification that our visible bounds relative to the root has changed.
  // This updates the bounds of the HWND.
  virtual void VisibleBoundsInRootChanged();

 private:
  // The hosted window handle.
  HWND hwnd_;

  // The preferred size of this View
  CSize preferred_size_;

  // Have we installed a region on the HWND used to clip to only the visible
  // portion of the HWND?
  bool installed_clip_;

  // Fast resizing will move the hwnd and clip its window region, this will
  // result in white areas and will not resize the content (so scrollbars
  // will be all wrong and content will flow offscreen). Only use this
  // when you're doing extremely quick, high-framerate vertical resizes
  // and don't care about accuracy. Make sure you do a real resize at the
  // end. USE WITH CAUTION.
  bool fast_resize_;

  // The view that should be given focus when this HWNDView is focused.
  View* focus_view_;
};

}

#endif // CHROME_VIEWS_HWND_VIEW_H__
