// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/window.h"

class AeroGlassNonClientView;
class BrowserView;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame
//
//  AeroGlassFrame is a Window subclass that provides the window frame on
//  Windows Vista with DWM desktop compositing enabled. The window's non-client
//  areas are drawn by the system.
//
class AeroGlassFrame : public BrowserFrame,
                       public views::Window {
 public:
  explicit AeroGlassFrame(BrowserView* browser_view);
  virtual ~AeroGlassFrame();

  void Init();

  // Determine the distance of the left edge of the minimize button from the
  // right edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Overridden from BrowserFrame:
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds);
  virtual void SizeToContents(const gfx::Rect& contents_bounds) {}
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual views::Window* GetWindow();

 protected:
  // Overridden from views::WidgetWin:
  virtual bool AcceleratorPressed(views::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& pt);

  // Overridden from views::CustomFrameWindow:
  virtual int GetShowState() const;

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Return a pointer to the concrete type of our non-client view.
  AeroGlassNonClientView* GetAeroGlassNonClientView() const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  bool frame_initialized_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_

