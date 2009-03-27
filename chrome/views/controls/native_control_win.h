// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_NATIVE_CONTROL_WIN_H_
#define CHROME_VIEWS_CONTROLS_NATIVE_CONTROL_WIN_H_

#include "chrome/views/controls/hwnd_view.h"

namespace views {

// A View that hosts a native Windows control.
class NativeControlWin : public HWNDView {
 public:
  static const wchar_t* kNativeControlWinKey;

  NativeControlWin();
  virtual ~NativeControlWin();

  // Called by the containing WidgetWin when a message is received from the HWND
  // created by an object derived from NativeControlWin. Derived classes MUST
  // call _this_ version of the function if they override it and do not handle
  // all of the messages listed in widget_win.cc ProcessNativeControlWinMessage.
  // Returns true if the message was handled, with a valid result in |result|.
  // Returns false if the message was not handled.
  virtual bool ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 LRESULT* result);

  // Called by our subclassed window procedure when a WM_KEYDOWN message is
  // received by the HWND created by an object derived from NativeControlWin.
  // Returns true if the key was processed, false otherwise.
  virtual bool OnKeyDown(int vkey) { return false; }

  // Overridden from View:
  virtual void SetEnabled(bool enabled);

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);
  virtual void VisibilityChanged(View* starting_from, bool is_visible);
  virtual void Focus();

  // Called by the containing WidgetWin when a WM_CONTEXTMENU message is
  // received from the HWND created by an object derived from NativeControlWin.
  virtual void ShowContextMenu(const gfx::Point& location);

  // Derived classes interested in receiving key down notification should
  // override this method and return true.  In which case OnKeyDown is called
  // when a key down message is sent to the control.
  // Note that this method is called at the time of the control creation: the
  // behavior will not change if the returned value changes after the control
  // has been created.
  virtual bool NotifyOnKeyDown() const { return false; }

  // Called when the NativeControlWin is attached to a View hierarchy with a
  // valid Widget. The NativeControlWin should use this opportunity to create
  // its associated HWND.
  virtual void CreateNativeControl() = 0;

  // MUST be called by the subclass implementation of |CreateNativeControl|
  // immediately after creating the control HWND, otherwise it won't be attached
  // to the HWNDView and will be effectively orphaned.
  virtual void NativeControlCreated(HWND native_control);

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

 private:
  // Called by the containing WidgetWin when a message of type WM_CTLCOLORBTN or
  // WM_CTLCOLORSTATIC is sent from the HWND created by an object dreived from
  // NativeControlWin.
  LRESULT GetControlColor(UINT message, HDC dc, HWND sender);

  // Our subclass window procedure for the attached control.
  static LRESULT CALLBACK NativeControlWndProc(HWND window,
                                               UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param);

  // The window procedure before we subclassed.
  static WNDPROC original_wndproc_;

  DISALLOW_COPY_AND_ASSIGN(NativeControlWin);
};
  
}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_NATIVE_CONTROL_WIN_H_
