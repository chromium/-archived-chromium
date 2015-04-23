// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/glass_browser_frame_view.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip_wrapper.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "views/window/client_view.h"
#include "views/window/window_resources.h"

// static
SkBitmap* GlassBrowserFrameView::distributor_logo_ = NULL;
HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

namespace {
// There are 3 px of client edge drawn inside the outer frame borders.
const int kNonClientBorderThickness = 3;
// Besides the frame border, there's another 11 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of the top and bottom edges triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The distributor logo is drawn 3 px from the top of the window.
static const int kLogoTopSpacing = 3;
// In maximized mode, the OTR avatar starts 2 px below the top of the screen, so
// that it doesn't extend into the "3D edge" portion of the titlebar.
const int kOTRMaximizedTopSpacing = 2;
// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;
// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;
// The top 1 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 1;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;
// When there's a distributor logo, we leave a 7 px gap between it and the
// caption buttons.
const int kLogoCaptionSpacing = 7;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      frame_(frame),
      browser_view_(browser_view),
      throbber_running_(false),
      throbber_frame_(0) {
  InitClass();
  if (frame_->GetWindow()->GetDelegate()->ShouldShowWindowIcon())
    InitThrobberIcons();
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStrip(
    TabStripWrapper* tabstrip) const {
  int minimize_button_offset = frame_->GetMinimizeButtonOffset();
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_bounds_.right() + kOTRSideSpacing) :
      NonClientBorderThickness();
  // minimize_button_offset assumes LTR layout since the window controls
  // themselves are not flipped, so we need to adjust the tabstrip's x
  // position for them in RTL languages.
  if (UILayoutIsRightToLeft())
    tabstrip_x += (width() - minimize_button_offset);
  int tabstrip_width = minimize_button_offset - tabstrip_x -
      (frame_->GetWindow()->IsMaximized() ? kNewTabCaptionMaximizedSpacing
                                          : kNewTabCaptionRestoredSpacing);
  if (UILayoutIsRightToLeft())
    tabstrip_width += tabstrip_x;
  return gfx::Rect(tabstrip_x, NonClientTopBorderHeight(),
                   std::max(0, tabstrip_width),
                   tabstrip->GetPreferredHeight());
}

void GlassBrowserFrameView::UpdateThrobber(bool running) {
  if (throbber_running_) {
    if (running) {
      DisplayNextThrobberFrame();
    } else {
      StopThrobber();
    }
  } else if (running) {
    StartThrobber();
  }
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = frame_->GetWindow()->GetNativeWindow();
  if (!browser_view_->IsTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-NULL window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

gfx::Point GlassBrowserFrameView::GetSystemMenuPoint() const {
  views::Window* window = frame_->GetWindow();
  gfx::Point system_menu_point;
  if (browser_view_->IsBrowserTypeNormal()) {
    // The X coordinate conditional is because in maximized mode the frame edge
    // and the client edge are both offscreen, whereas in the opaque frame
    // (where we don't do this trick) maximized windows have no client edge and
    // only the frame edge is offscreen.
    system_menu_point.SetPoint(NonClientBorderThickness() -
        ((window->IsMaximized() || window->IsFullscreen()) ?
         0 : kClientEdgeThickness),
        NonClientTopBorderHeight() + browser_view_->GetTabStripHeight() -
        (frame_->GetWindow()->IsFullscreen() ? 0 : kClientEdgeThickness));
  } else {
    system_menu_point.SetPoint(0, -kFrameShadowThickness);
  }
  ConvertPointToScreen(this, &system_menu_point);
  return system_menu_point;
}

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  // If the browser isn't in normal mode, we haven't customized the frame, so
  // Windows can figure this out.  If the point isn't within our bounds, then
  // it's in the native portion of the frame, so again Windows can figure it
  // out.
  if (!browser_view_->IsBrowserTypeNormal() || !bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int border_thickness = FrameBorderThickness();
  int window_component = GetHTComponentForFrame(point, border_thickness,
      NonClientBorderThickness(), border_thickness,
      kResizeAreaCornerSize - border_thickness,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

void GlassBrowserFrameView::Paint(gfx::Canvas* canvas) {
  if (!browser_view_->IsTabStripVisible())
    return;  // Nothing is visible, so don't bother to paint.

  PaintDistributorLogo(canvas);
  PaintToolbarBackground(canvas);
  PaintOTRAvatar(canvas);
  if (!frame_->GetWindow()->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  LayoutDistributorLogo();
  LayoutOTRAvatar();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

int GlassBrowserFrameView::FrameBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  return (window->IsMaximized() || window->IsFullscreen()) ?
      0 : GetSystemMetrics(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::NonClientBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  return (window->IsMaximized() || window->IsFullscreen()) ?
      0 : kNonClientBorderThickness;
}

int GlassBrowserFrameView::NonClientTopBorderHeight() const {
  if (frame_->GetWindow()->IsFullscreen())
    return 0;
  // We'd like to use FrameBorderThickness() here, but the maximized Aero glass
  // frame has a 0 frame border around most edges and a CXSIZEFRAME-thick border
  // at the top (see AeroGlassFrame::OnGetMinMaxInfo()).
  return GetSystemMetrics(SM_CXSIZEFRAME) + (browser_view_->IsMaximized() ?
      -kTabstripTopShadowThickness : kNonClientRestoredExtraThickness);
}

void GlassBrowserFrameView::PaintDistributorLogo(gfx::Canvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized and
  // when we actually have a logo.
  if (!frame_->GetWindow()->IsMaximized() && distributor_logo_ &&
      browser_view_->ShouldShowDistributorLogo()) {
    // NOTE: We don't mirror the logo placement here because the outer frame
    // itself isn't mirrored in RTL.  This is a bug; if it is fixed, this should
    // be mirrored as in opaque_non_client_view.cc.
    canvas->DrawBitmapInt(*distributor_logo_, logo_bounds_.x(),
                          logo_bounds_.y());
  }
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                           this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // Draw the toolbar background, setting src_y of the paint to the tab
  // strip height as the toolbar background begins at the top of the tabs.
  canvas->TileImageInt(*theme_toolbar,
      0, browser_view_->GetTabStripHeight() - 1,
      toolbar_bounds.x() - 1, toolbar_bounds.y() + 2,
      toolbar_bounds.width() + 2, theme_toolbar->height());

  SkBitmap* toolbar_left =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  canvas->DrawBitmapInt(*toolbar_left,
                        toolbar_bounds.x() - toolbar_left->width(),
                        toolbar_bounds.y());

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center, toolbar_bounds.x(), toolbar_bounds.y(),
                       toolbar_bounds.width(), toolbar_center->height());

  canvas->DrawBitmapInt(*tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
      toolbar_bounds.right(), toolbar_bounds.y());

  // Draw the content/toolbar separator.
  canvas->DrawLineInt(ResourceBundle::toolbar_separator_color,
      toolbar_bounds.x(), toolbar_bounds.bottom() - 1,
      toolbar_bounds.right() - 1, toolbar_bounds.bottom() - 1);
}

void GlassBrowserFrameView::PaintOTRAvatar(gfx::Canvas* canvas) {
  if (!browser_view_->ShouldShowOffTheRecordAvatar())
    return;

  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  canvas->DrawBitmapInt(otr_avatar_icon, 0,
      (otr_avatar_icon.height() - otr_avatar_bounds_.height()) / 2,
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(),
      MirroredLeftPointForRect(otr_avatar_bounds_), otr_avatar_bounds_.y(),
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), false);
}

void GlassBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  // The client edges start below the toolbar upper corner images regardless
  // of how tall the toolbar itself is.
  int client_area_top =
      frame_->GetWindow()->GetClientView()->y() +
      browser_view_->GetToolbarBounds().y() +
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height();

  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());
  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness());
  int client_area_height = client_area_bottom - client_area_top;
  SkBitmap* right = tp->GetBitmapNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);

  // Draw the toolbar color so that the one pixel areas down the sides
  // show the right color even if not covered by the toolbar image.
  SkColor toolbar_color = tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_top,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_top);

  canvas->DrawBitmapInt(
      *tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
      client_area_bounds.right(), client_area_bottom);

  SkBitmap* bottom = tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());

  SkBitmap* bottom_left =
      tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  SkBitmap* left = tp->GetBitmapNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);
}

void GlassBrowserFrameView::LayoutDistributorLogo() {
  if (distributor_logo_ && browser_view_->ShouldShowDistributorLogo()) {
    logo_bounds_.SetRect(frame_->GetMinimizeButtonOffset() -
        distributor_logo_->width() - kLogoCaptionSpacing, kLogoTopSpacing,
        distributor_logo_->width(), distributor_logo_->height());
  } else {
    logo_bounds_.SetRect(frame_->GetMinimizeButtonOffset(), kLogoTopSpacing, 0,
                         0);
  }
}

void GlassBrowserFrameView::LayoutOTRAvatar() {
  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  int top_height = NonClientTopBorderHeight();
  int tabstrip_height, otr_height;
  if (browser_view_->IsTabStripVisible()) {
    tabstrip_height = browser_view_->GetTabStripHeight() - kOTRBottomSpacing;
    otr_height = frame_->GetWindow()->IsMaximized() ?
        (tabstrip_height - kOTRMaximizedTopSpacing) :
        otr_avatar_icon.height();
  } else {
    tabstrip_height = otr_height = 0;
  }
  otr_avatar_bounds_.SetRect(NonClientBorderThickness() + kOTRSideSpacing,
                             top_height + tabstrip_height - otr_height,
                             otr_avatar_icon.width(), otr_height);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds(int width,
                                                           int height) const {
  if (!browser_view_->IsTabStripVisible())
    return gfx::Rect(0, 0, this->width(), this->height());

  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

void GlassBrowserFrameView::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;

    // This will reset the small icon which we set in the throbber code.
    // Windows will then pick up the default icon from the window class.
    SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL), NULL);
  }
}

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] = rb.LoadThemeIcon(IDR_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}

// static
void GlassBrowserFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
#if defined(GOOGLE_CHROME_BUILD)
    distributor_logo_ = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_DISTRIBUTOR_LOGO);
#endif

    initialized = true;
  }
}
