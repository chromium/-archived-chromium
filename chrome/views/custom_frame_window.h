// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__
#define CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"

namespace ChromeViews {

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

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

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

  // Overridden from HWNDViewContainer:
  virtual void OnInitMenu(HMENU menu);
  virtual void OnMouseLeave();
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& point);
  virtual LRESULT OnNCMouseMove(UINT flags, const CPoint& point);
  virtual void OnNCPaint(HRGN rgn);
  virtual void OnNCRButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCLButtonDown(UINT flags, const CPoint& point);
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message);
  virtual void OnSize(UINT param, const CSize& size);

 private:
  // Shows the system menu at the specified screen point.
  void RunSystemMenu(const CPoint& point);

  // Resets the window region.
  void ResetWindowRegion();

  // True if this window is the active top level window.
  bool is_active_;

  // Static resource initialization.
  static void InitClass();
  enum ResizeCursor {
    RC_NORMAL = 0, RC_VERTICAL, RC_HORIZONTAL, RC_NESW, RC_NWSE
  };
  static HCURSOR resize_cursors_[6];

  DISALLOW_EVIL_CONSTRUCTORS(CustomFrameWindow);
};

}

#endif  // CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__

