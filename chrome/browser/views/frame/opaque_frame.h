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

#ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/custom_frame_window.h"

class BrowserView2;
namespace ChromeViews {
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
                    public ChromeViews::CustomFrameWindow {
 public:
  explicit OpaqueFrame(BrowserView2* browser_view);
  virtual ~OpaqueFrame();

  bool IsToolbarVisible() const;
  bool IsTabStripVisible() const;

  // Returns bounds of various areas within the BrowserView ClientView.
  gfx::Rect GetToolbarBounds() const;
  gfx::Rect GetContentsBounds() const;

 protected:
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
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, const RECT* new_bounds);

 private:
  // Return a pointer to the concrete type of our non-client view.
  OpaqueNonClientView* GetOpaqueNonClientView() const;

  // The BrowserView2 is our ClientView. This is a pointer to it.
  BrowserView2* browser_view_;

  DISALLOW_EVIL_CONSTRUCTORS(OpaqueFrame);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_FRAME_H_
