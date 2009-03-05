// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_HWND_VIEW_H__
#define CHROME_VIEWS_HWND_VIEW_H__

#include <string>

#include "chrome/views/view.h"

namespace views {

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

  virtual gfx::Size GetPreferredSize();

  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

  // Attach a window handle to this View, making the window it represents
  // subject to sizing according to this View's parent container's Layout
  // Manager's sizing heuristics.
  void Attach(HWND hwnd);

  // Detach the attached window handle. It will no longer be updated
  void Detach();

  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
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

  virtual void Focus();

 private:
  // The hosted window handle.
  HWND hwnd_;

  // The preferred size of this View
  gfx::Size preferred_size_;

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

}  // namespace views

#endif // CHROME_VIEWS_HWND_VIEW_H__

