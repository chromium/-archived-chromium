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

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/window.h"

class AeroGlassNonClientView;
class BrowserView2;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame
//
//  AeroGlassFrame is a Window subclass that provides the window frame on
//  Windows Vista with DWM desktop compositing enabled. The window's non-client
//  areas are drawn by the system.
//
class AeroGlassFrame : public BrowserFrame,
                       public ChromeViews::Window {
 public:
  explicit AeroGlassFrame(BrowserView2* browser_view);
  virtual ~AeroGlassFrame();

  void Init(const gfx::Rect& bounds);

  bool IsToolbarVisible() const;
  bool IsTabStripVisible() const;

  // Returns bounds of various areas within the BrowserView ClientView.
  gfx::Rect GetToolbarBounds() const;
  gfx::Rect GetContentsBounds() const;

  // Determine the distance of the left edge of the minimize button from the
  // right edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Overridden from BrowserFrame:
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const;
  virtual ChromeViews::Window* GetWindow();

  // Overridden from ChromeViews::HWNDViewContainer:
  virtual bool AcceleratorPressed(ChromeViews::Accelerator* accelerator);
  virtual bool GetAccelerator(int cmd_id,
                              ChromeViews::Accelerator* accelerator);

 protected:
  // Overridden from ChromeViews::HWNDViewContainer:
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& pt);

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Return a pointer to the concrete type of our non-client view.
  AeroGlassNonClientView* GetAeroGlassNonClientView() const;

  // The BrowserView2 is our ClientView. This is a pointer to it.
  BrowserView2* browser_view_;

  bool frame_initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_FRAME_H_
