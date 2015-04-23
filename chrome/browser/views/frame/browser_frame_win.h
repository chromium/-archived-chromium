// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_WIN_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_WIN_

#include "base/basictypes.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "views/window/window_win.h"

class AeroGlassNonClientView;
class BrowserNonClientFrameView;
class BrowserRootView;
class BrowserTabStrip;
class BrowserView;
class NonClientFrameView;
class Profile;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin
//
//  BrowserFrame is a WindowWin subclass that provides the window frame for the
//  Chrome browser window.
//
class BrowserFrameWin : public BrowserFrame, public views::WindowWin {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameWin(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameWin();

  // This initialization function must be called after construction, it is
  // separate to avoid recursive calling of the frame from its constructor.
  void Init();

  // BrowserFrame implementation.
  virtual views::Window* GetWindow();
  virtual void TabStripCreated(TabStripWrapper* tabstrip);
  virtual int GetMinimizeButtonOffset() const;
  virtual gfx::Rect GetBoundsForTabStrip(TabStripWrapper* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual void ContinueDraggingDetachedTab();
  virtual ThemeProvider* GetThemeProviderForFrame() const;

  // Overridden from views::Widget.
  virtual ThemeProvider* GetThemeProvider() const;
  virtual ThemeProvider* GetDefaultThemeProvider() const;

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from views::WidgetWin:
  virtual bool AcceleratorPressed(views::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual void OnEnterSizeMove();
  virtual void OnExitSizeMove();
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& pt);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);

  // Overridden from views::Window:
  virtual int GetShowState() const;
  virtual bool IsAppWindow() const { return true; }
  virtual views::NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual views::RootView* CreateRootView();

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Update the window's pacity when entering and exiting detached dragging
  // mode.
  void UpdateWindowAlphaForTabDragging(bool dragging);

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  // An unowning reference to the root view associated with the window. We save
  // a copy as a BrowserRootView to avoid evil casting later, when we need to
  // call functions that only exist on BrowserRootView (versus RootView).
  BrowserRootView* root_view_;

  bool frame_initialized_;

  Profile* profile_;

  // The window styles before we modified them for a tab dragging operation.
  DWORD saved_window_style_;
  DWORD saved_window_ex_style_;

  // True if the window is currently being moved in a detached tab drag
  // operation.
  bool detached_drag_mode_;

  // When this frame represents a detached tab being dragged, this is a TabStrip
  // in another window that the tab being dragged would be docked to if the
  // mouse were released, or NULL if there is no suitable TabStrip.
  BrowserTabStrip* drop_tabstrip_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_WIN_
