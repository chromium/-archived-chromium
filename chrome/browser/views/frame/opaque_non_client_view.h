// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_

#include "chrome/browser/views/frame/opaque_frame.h"
#include "chrome/browser/views/tab_icon_view.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/button.h"

class BrowserView2;
class OpaqueFrame;
class TabContents;
class TabStrip;
class WindowResources;

class OpaqueNonClientView : public ChromeViews::NonClientView,
                            public ChromeViews::BaseButton::ButtonListener,
                            public TabIconView::TabContentsProvider {
 public:
  // Constructs a non-client view for an OpaqueFrame. |is_otr| specifies if the
  // frame was created "off-the-record" and as such different bitmaps should be
  // used to render the frame.
  OpaqueNonClientView(OpaqueFrame* frame, BrowserView2* browser_view);
  virtual ~OpaqueNonClientView();

  // Retrieve the bounds of the window for the specified contents bounds.
  gfx::Rect GetWindowBoundsForClientBounds(const gfx::Rect& client_bounds);

  // Retrieve the bounds (in ClientView coordinates) that the specified
  // |tabstrip| will be laid out within.
  gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip);

  // Updates the window icon/throbber.
  void UpdateWindowIcon();

 protected:
  // Overridden from TabIconView::TabContentsProvider:
  virtual TabContents* GetCurrentTabContents();
  virtual SkBitmap GetFavIcon();

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
  virtual bool GetAccessibleRole(VARIANT* role);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual void SetAccessibleName(const std::wstring& name);

 private:
  // Updates the system menu icon button.
  void SetWindowIcon(SkBitmap window_icon);

  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateNonClientTopHeight() const;

  // Paint various sub-components of this view.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintOTRAvatar(ChromeCanvas* canvas);
  void PaintDistributorLogo(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintToolbarBackground(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutOTRAvatar();
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

  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // The layout rect of the OTR avatar icon, if visible.
  gfx::Rect otr_avatar_bounds_;

  // Window controls.
  ChromeViews::Button* minimize_button_;
  ChromeViews::Button* maximize_button_;
  ChromeViews::Button* restore_button_;
  ChromeViews::Button* close_button_;

  // The Window icon.
  TabIconView* window_icon_;

  // The frame that hosts this view.
  OpaqueFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView2* browser_view_;

  // The resources currently used to paint this view.
  WindowResources* current_active_resources_;
  WindowResources* current_inactive_resources_;

  // The accessible name of this view.
  std::wstring accessible_name_;

  static void InitClass();
  static SkBitmap distributor_logo_;
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static WindowResources* active_otr_resources_;
  static WindowResources* inactive_otr_resources_;

  DISALLOW_EVIL_CONSTRUCTORS(OpaqueNonClientView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_OPAQUE_NON_CLIENT_VIEW_H_

