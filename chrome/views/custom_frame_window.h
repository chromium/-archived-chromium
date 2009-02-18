// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__
#define CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"

namespace views {

class NonClientView;

////////////////////////////////////////////////////////////////////////////////
//
// CustomFrameWindow
//
//  A CustomFrameWindow is a Window subclass that implements the Chrome-style
//  window frame used on Windows XP and Vista without DWM Composition.
//  See documentation in window.h for more information about the capabilities
//  of this window type.
//
////////////////////////////////////////////////////////////////////////////////
class CustomFrameWindow : public Window {
 public:
  explicit CustomFrameWindow(WindowDelegate* window_delegate);
  CustomFrameWindow(WindowDelegate* window_delegate,
                    NonClientView* non_client_view);
  virtual ~CustomFrameWindow();

  // Returns whether or not the frame is active.
  bool is_active() const { return is_active_; }

  // Overridden from Window:
  virtual void Init(HWND parent, const gfx::Rect& bounds);
  virtual void SetClientView(ClientView* client_view);
  virtual gfx::Size CalculateWindowSizeForClientSize(
      const gfx::Size& client_size) const;
  virtual void UpdateWindowTitle();
  virtual void UpdateWindowIcon();

 protected:
  // Overridden from Window:
  virtual void SizeWindowToDefault();
  virtual void EnableClose(bool enable);
  virtual void DisableInactiveRendering(bool disable);

  // Overridden from WidgetWin:
  virtual void OnInitMenu(HMENU menu);
  virtual void OnMouseLeave();
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& point);
  virtual void OnNCPaint(HRGN rgn);
  virtual void OnNCLButtonDown(UINT ht_component, const CPoint& point);
  virtual LRESULT OnNCUAHDrawCaption(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message);
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  virtual LRESULT OnSetText(const wchar_t* text);
  virtual void OnSize(UINT param, const CSize& size);
  virtual void OnSysCommand(UINT notification_code, CPoint click);

 private:
  class ScopedRedrawLock;

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  void LockUpdates();
  void UnlockUpdates();

  // Resets the window region.
  void ResetWindowRegion();

  // Converts a non-client mouse down message to a regular ChromeViews event
  // and handle it. |point| is the mouse position of the message in screen
  // coords. |flags| are flags that would be passed with a WM_L/M/RBUTTON*
  // message and relate to things like which button was pressed. These are
  // combined with flags relating to the current key state.
  void ProcessNCMousePress(const CPoint& point, int flags);

  // True if this window is the active top level window.
  bool is_active_;

  // True if updates to this window are currently locked.
  bool lock_updates_;

  // The window styles of the window before updates were locked.
  DWORD saved_window_style_;

  // Static resource initialization.
  static void InitClass();
  enum ResizeCursor {
    RC_NORMAL = 0, RC_VERTICAL, RC_HORIZONTAL, RC_NESW, RC_NWSE
  };
  static HCURSOR resize_cursors_[6];

  DISALLOW_EVIL_CONSTRUCTORS(CustomFrameWindow);
};

}  // namespace views

#endif  // CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__

