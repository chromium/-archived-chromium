// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OLD_FRAMES_SIMPLE_VISTA_FRAME_H__
#define CHROME_BROWSER_VIEWS_OLD_FRAMES_SIMPLE_VISTA_FRAME_H__

#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/old_frames/vista_frame.h"

class WebAppIconManager;

////////////////////////////////////////////////////////////////////////////////
//
// A simple vista frame that contains a browser object. This frame doesn't show
// any tab. It is used for web applications. It will likely be used in the
// future for detached popups.
//
// This window simply uses the traditional Vista look and feel.
//
////////////////////////////////////////////////////////////////////////////////
class SimpleVistaFrame : public VistaFrame,
                         public LocationBarView::Delegate {
 public:
  // Invoked by ChromeFrame::CreateChromeFrame to create a new SimpleVistaFrame.
  // An empty |bounds| means that Windows should decide where to place the
  // window.
  static SimpleVistaFrame* CreateFrame(const gfx::Rect& bounds,
                                       Browser* browser);

  virtual ~SimpleVistaFrame();

  virtual void Init();

  // LocationBarView delegate.
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

 protected:
  // Overridden from VistaFrame.
  virtual bool IsTabStripVisible() const { return false; }
  virtual bool IsToolBarVisible() const { return false; }
  virtual bool SupportsBookmarkBar() const { return false; }
  virtual void SetWindowTitle(const std::wstring& title);
  virtual void ShowTabContents(TabContents* selected_contents);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual LRESULT OnNCHitTest(const CPoint& pt);
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param);
  virtual void OnNCLButtonDown(UINT flags, const CPoint& pt);
  virtual void ValidateThrobber();

  virtual void Layout();

  // Overriden to create the WebAppIconManager, then invoke super.
  virtual void InitAfterHWNDCreated();

 private:
  explicit SimpleVistaFrame(Browser* browser);

  void StartThrobber();
  bool IsThrobberRunning();
  void DisplayNextThrobberFrame();
  void StopThrobber();

  // Update the location bar if it is visible.
  void UpdateLocationBar();

  // Set the current window icon. Use NULL for a default icon.
  void SetCurrentIcon(HICON icon);

  // We change the window icon for the throbber.
  bool throbber_running_;

  // Current throbber frame.
  int throbber_frame_;

  // The optional location bar for popup windows.
  LocationBarView* location_bar_;

  scoped_ptr<WebAppIconManager> icon_manager_;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleVistaFrame);
};
#endif  // CHROME_BROWSER_VIEWS_OLD_FRAMES_SIMPLE_VISTA_FRAME_H__

