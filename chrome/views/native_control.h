// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_NATIVE_CONTROL_H__
#define CHROME_VIEWS_NATIVE_CONTROL_H__

#include <windows.h>

#include "chrome/views/view.h"

namespace views {

class HWNDView;
class NativeControlContainer;
class RootView;

////////////////////////////////////////////////////////////////////////////////
//
// NativeControl is an abstract view that is used to implement views wrapping
// native controls. Subclasses can simply implement CreateNativeControl() to
// wrap a new kind of control
//
////////////////////////////////////////////////////////////////////////////////
class NativeControl : public View {
 public:
   enum Alignment {
     LEADING = 0,
     CENTER,
     TRAILING };

  NativeControl();
  virtual ~NativeControl();

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);
  virtual void Layout();

  // Overridden to properly set the native control state.
  virtual void SetVisible(bool f);
  virtual void SetEnabled(bool enabled);

  // Overridden to do nothing.
  virtual void Paint(ChromeCanvas* canvas);
 protected:
  friend class NativeControlContainer;

  // Overridden by sub-classes to create the windows control which is wrapped
  virtual HWND CreateNativeControl(HWND parent_container) = 0;

  // Invoked when the native control sends a WM_NOTIFY message to its parent
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param) = 0;

  // Invoked when the native control sends a WM_COMMAND message to its parent
  virtual LRESULT OnCommand(UINT code, int id, HWND source) { return 0; }

  // Invoked when the appropriate gesture for a context menu is issued.
  virtual void OnContextMenu(const CPoint& location);

  // Overridden so to set the native focus to the native control.
  virtual void Focus();

  // Invoked when the native control sends a WM_DESTORY message to its parent.
  virtual void OnDestroy() { }

  // Return the native control
  virtual HWND GetNativeControlHWND();

  // Invoked by the native windows control when it has been destroyed. This is
  // invoked AFTER WM_DESTORY has been sent. Any window commands send to the
  // HWND will most likely fail.
  void NativeControlDestroyed();

  // Overridden so that the control properly reflects parent's visibility.
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  // Controls that have fixed sizes should call these methods to specify the
  // actual size and how they should be aligned within their parent.
  void SetFixedWidth(int width, Alignment alignment);
  void SetFixedHeight(int height, Alignment alignment);

  // Derived classes interested in receiving key down notification should
  // override this method and return true.  In which case OnKeyDown is called
  // when a key down message is sent to the control.
  // Note that this method is called at the time of the control creation: the
  // behavior will not change if the returned value changes after the control
  // has been created.
  virtual bool NotifyOnKeyDown() const { return false; }

  // Invoked when a key is pressed on the control (if NotifyOnKeyDown returns
  // true).  Should return true if the key message was processed, false
  // otherwise.
  virtual bool OnKeyDown(int virtual_key_code) { return false; }

  // Returns additional extended style flags. When subclasses call
  // CreateWindowEx in order to create the underlying control, they must OR the
  // ExStyle parameter with the value returned by this function.
  //
  // We currently use this method in order to add flags such as WS_EX_LAYOUTRTL
  // to the HWND for views with right-to-left UI layout.
  DWORD GetAdditionalExStyle() const;

  // TODO(xji): we use the following temporary function as we transition the
  // various native controls to use the right set of RTL flags. This function
  // will go away (and be replaced by GetAdditionalExStyle()) once all the
  // controls are properly transitioned.
  DWORD GetAdditionalRTLStyle() const;

  // This variable is protected to provide subclassers direct access. However
  // subclassers should always check for NULL since this variable is only
  // initialized in ValidateNativeControl().
  HWNDView* hwnd_view_;

  // Fixed size information.  -1 for a size means no fixed size.
  int fixed_width_;
  Alignment horizontal_alignment_;
  int fixed_height_;
  Alignment vertical_alignment_;

 private:

  void ValidateNativeControl();

  static LRESULT CALLBACK NativeControlWndProc(HWND window, UINT message,
                                               WPARAM w_param, LPARAM l_param);

  NativeControlContainer* container_;

  DISALLOW_EVIL_CONSTRUCTORS(NativeControl);
};

}  // namespace views

#endif  // CHROME_VIEWS_NATIVE_CONTROL_H__

