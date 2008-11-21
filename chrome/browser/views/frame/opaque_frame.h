// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/custom_frame_window.h"

class BrowserView;
namespace views {
class Window;
}
class OpaqueNonClientView;
class TabStrip;

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame
//
//  OpaqueFrame is a CustomFrameWindow subclass that in conjunction with
//  OpaqueNonClientView provides the window frame on Windows XP and on Windows
//  Vista when DWM desktop compositing is disabled. The window title and
//  borders are provided with bitmaps.
//
class OpaqueFrame : public BrowserFrame,
                    public views::CustomFrameWindow {
 public:
  explicit OpaqueFrame(BrowserView* browser_view);
  virtual ~OpaqueFrame();

  void Init();

 protected:
  // Overridden from BrowserFrame:
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual views::Window* GetWindow();

  // Overridden from views::CustomFrameWindow:
  virtual void UpdateWindowIcon();
  virtual int GetShowState() const;

  // Overridden from views::WidgetWin:
  virtual bool AcceleratorPressed(views::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual void OnSysCommand(UINT notification_code, CPoint click);

 private:
  // Return a pointer to the concrete type of our non-client view.
  OpaqueNonClientView* GetOpaqueNonClientView() const;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  DISALLOW_EVIL_CONSTRUCTORS(OpaqueFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_

