// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_

#include "chrome/views/window_win.h"

class AeroGlassNonClientView;
class BrowserView;
class NonClientFrameView;
class TabStrip;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  BrowserNonClientFrameView() : NonClientFrameView() {}
  virtual ~BrowserNonClientFrameView() {}

  // Returns the bounds within which the TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame
//
//  BrowserFrame is a WindowWin subclass that provides the window frame for the
//  Chrome browser window.
//
class BrowserFrame : public views::WindowWin {
 public:
  explicit BrowserFrame(BrowserView* browser_view);
  virtual ~BrowserFrame();

  // Initialize the frame. Creates the Window.
  void Init();

  // Determine the distance of the left edge of the minimize button from the
  // left edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Retrieves the bounds, in non-client view coordinates for the specified
  // TabStrip.
  gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const;

  // Tells the frame to update the throbber.
  void UpdateThrobber(bool running);

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from views::WidgetWin:
  virtual bool AcceleratorPressed(views::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);
  virtual void OnEnterSizeMove();
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& pt);

  // Overridden from views::Window:
  virtual int GetShowState() const;
  virtual bool IsAppWindow() const { return true; }
  virtual views::NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual views::RootView* CreateRootView();

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  bool frame_initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_
