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

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_

#include "chrome/browser/views/frame/aero_glass_frame.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/button.h"

class WindowResources;

class AeroGlassNonClientView : public ChromeViews::NonClientView {
 public:
  // Constructs a non-client view for an AeroGlassFrame.
  explicit AeroGlassNonClientView(AeroGlassFrame* frame);
  virtual ~AeroGlassNonClientView();

 protected:
  // Overridden from ChromeViews::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);

  // Overridden from ChromeViews::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateNonClientTopHeight() const;

  // Paint various sub-components of this view.
  void PaintDistributorLogo(ChromeCanvas* canvas);
  void PaintToolbarBackground(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutDistributorLogo();
  void LayoutClientView();
 
  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // The frame that hosts this view.
  AeroGlassFrame* frame_;

  static void InitClass();
  static SkBitmap distributor_logo_;
  static WindowResources* resources_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassNonClientView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_
