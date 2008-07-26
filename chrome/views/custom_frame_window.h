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

#ifndef CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__
#define CHROME_VIEWS_CUSTOM_FRAME_WINDOW_H__

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"

namespace ChromeViews {

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
  CustomFrameWindow();
  class NonClientView;
  explicit CustomFrameWindow(NonClientView* non_client_view);
  virtual ~CustomFrameWindow();

  // Create the CustomFrameWindow.
  // The parent of this window is always the desktop, however the owner is a
  // window that this window is dependent on, if this window is opened as a
  // modal dialog or dependent window. This is NULL if the window is not
  // dependent on any other window.
  // |contents_view| is the view to be displayed in the client area of the
  // window.
  // |window_delegate| is an object implementing the WindowDelegate interface
  // that supplies information to the window such as its title, icon, etc.
  virtual void Init(HWND owner,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    WindowDelegate* window_delegate);

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Returns whether or not the frame is active.
  bool is_active() const { return is_active_; }

  class NonClientView : public View {
   public:
    virtual void Init(ClientView* client_view) = 0;

    // Calculates the bounds of the client area of the window assuming the
    // window is sized to |width| and |height|.
    virtual gfx::Rect CalculateClientAreaBounds(int width,
                                                int height) const = 0;

    // Calculates the size of window required to display a client area of the
    // specified width and height.
    virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                       int height) const = 0;

    // Returns the point, in screen coordinates, where the system menu should
    // be shown so it shows up anchored to the system menu icon.
    virtual CPoint GetSystemMenuPoint() const = 0;

    // Determines the windows HT* code when the mouse cursor is at the
    // specified point, in window coordinates.
    virtual int HitTest(const gfx::Point& point) = 0;

    // Returns a mask to be used to clip the top level window for the given
    // size. This is used to create the non-rectangular window shape.
    virtual void GetWindowMask(const gfx::Size& size,
                               gfx::Path* window_mask) = 0;

    // Toggles the enable state for the Close button (and the Close menu item in
    // the system menu).
    virtual void EnableClose(bool enable) = 0;
  };

  // Overridden from Window:
  virtual gfx::Size CalculateWindowSizeForClientSize(
      const gfx::Size& client_size) const;
  virtual void UpdateWindowTitle();

 protected:
  // Overridden from Window:
  virtual void SizeWindowToDefault();
  virtual void EnableClose(bool enable);

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

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc).
  NonClientView* non_client_view_;

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
