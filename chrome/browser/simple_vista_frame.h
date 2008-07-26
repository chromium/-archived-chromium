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

#ifndef CHROME_BROWSER_SIMPLE_VISTA_FRAME_H__
#define CHROME_BROWSER_SIMPLE_VISTA_FRAME_H__

#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/vista_frame.h"

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
#endif  // CHROME_BROWSER_SIMPLE_VISTA_FRAME_H__
