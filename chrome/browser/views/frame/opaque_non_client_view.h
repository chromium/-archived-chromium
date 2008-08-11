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

#ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_

#include "chrome/browser/views/frame/opaque_frame.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/button.h"

class OpaqueFrame;
class TabStrip;
class WindowResources;

class OpaqueNonClientView : public ChromeViews::NonClientView,
                            public ChromeViews::BaseButton::ButtonListener {
 public:
  // Constructs a non-client view for an OpaqueFrame. |is_otr| specifies if the
  // frame was created "off-the-record" and as such different bitmaps should be
  // used to render the frame.
  OpaqueNonClientView(OpaqueFrame* frame, bool is_otr);
  virtual ~OpaqueNonClientView();

  // Retrieve the bounds of the window for the specified contents bounds.
  gfx::Rect GetWindowBoundsForClientBounds(const gfx::Rect& client_bounds);

  // Retrieve the bounds (in ClientView coordinates) that the specified
  // |tabstrip| will be laid out within.
  gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip);

 protected:
  // Overridden from ChromeViews::BaseButton::ButtonListener:
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

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
  virtual ChromeViews::View* GetViewForPoint(const CPoint& point,
                                             bool can_create_floating);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Updates the system menu icon button.
  void SetWindowIcon(SkBitmap window_icon);

  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateNonClientTopHeight() const;

  // Paint various sub-components of this view.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintDistributorLogo(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintToolbarBackground(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutDistributorLogo();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the set of resources to use to paint this view.
  WindowResources* resources() const {
    return frame_->is_active() || paint_as_active() ?
        current_active_resources_ : current_inactive_resources_;
  }
  
  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // The layout rect of the window icon, if visible.
  gfx::Rect icon_bounds_;

  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // Window controls.
  ChromeViews::Button* minimize_button_;
  ChromeViews::Button* maximize_button_;
  ChromeViews::Button* restore_button_;
  ChromeViews::Button* close_button_;

  // The frame that hosts this view.
  OpaqueFrame* frame_;

  // The BrowserView hosted within this View.

  // The resources currently used to paint this view.
  WindowResources* current_active_resources_;
  WindowResources* current_inactive_resources_;

  static void InitClass();
  static SkBitmap distributor_logo_;
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static WindowResources* active_otr_resources_;
  static WindowResources* inactive_otr_resources_;

  DISALLOW_EVIL_CONSTRUCTORS(OpaqueNonClientView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_
