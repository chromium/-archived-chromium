// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/views/tab_icon_view.h"
#include "views/controls/button/button.h"
#include "views/window/non_client_view.h"

class BrowserView;
namespace gfx {
class Font;
}
class TabContents;
class TabStrip;
namespace views {
class ImageButton;
}

class OpaqueBrowserFrameView : public BrowserNonClientFrameView,
                               public views::ButtonListener,
                               public TabIconView::TabIconViewModel {
 public:
  // Constructs a non-client view for an BrowserFrame. |is_otr| specifies if the
  // frame was created "off-the-record" and as such different bitmaps should be
  // used to render the frame.
  OpaqueBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~OpaqueBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(TabStripWrapper* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual gfx::Size GetMinimumSize();

 protected:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual gfx::Point GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual bool HitTest(const gfx::Point& l) const;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual void SetAccessibleName(const std::wstring& name);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const;
  virtual SkBitmap GetFavIconForTabIconView();

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the height of the top resize area.  This is smaller than the frame
  // border height in order to increase the window draggable area.
  int TopResizeHeight() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // The nonclient area at the top of the window may include some "unavailable"
  // pixels at its bottom: a dark shadow along the bottom of the titlebar and a
  // client edge.  These vary from mode to mode, so this function returns the
  // number of such pixels the nonclient height includes.
  int UnavailablePixelsAtBottomOfNonClientHeight() const;

  // Calculates multiple values related to title layout.  Returns the height of
  // the entire titlebar including any connected client edge.
  int TitleCoordinates(int* title_top_spacing_ptr,
                       int* title_thickness_ptr) const;

  // Calculates multiple values related to icon layout.  Returns the size of the
  // icon (along one edge).
  int IconSize(int* title_top_spacing_ptr,
               int* title_thickness_ptr,
               int* available_height_ptr) const;

  // Paint various sub-components of this view.  The *FrameBorder() functions
  // also paint the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintDistributorLogo(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintToolbarBackground(gfx::Canvas* canvas);
  void PaintOTRAvatar(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutDistributorLogo();
  void LayoutTitleBar();
  void LayoutOTRAvatar();
  void LayoutClientView();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // The layout rect of the OTR avatar icon, if visible.
  gfx::Rect otr_avatar_bounds_;

  // Window controls.
  views::ImageButton* minimize_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* close_button_;

  // The Window icon.
  TabIconView* window_icon_;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The accessible name of this view.
  std::wstring accessible_name_;

  static void InitClass();
  static void InitAppWindowResources();
  static SkBitmap* distributor_logo_;
  static gfx::Font* title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(OpaqueBrowserFrameView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
