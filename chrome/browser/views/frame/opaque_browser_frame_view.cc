// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/opaque_browser_frame_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"
#include "views/window/window_resources.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

// static
SkBitmap* OpaqueBrowserFrameView::distributor_logo_ = NULL;
gfx::Font* OpaqueBrowserFrameView::title_font_ = NULL;

namespace {
// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// Besides the frame border, there's another 11 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// While resize areas on Windows are normally the same size as the window
// borders, our top area is shrunk by 1 px to make it easier to move the window
// around with our thinner top grabbable strip.  (Incidentally, our side and
// bottom resize areas don't match the frame border thickness either -- they
// span the whole nonclient area, so there's no "dead zone" for the mouse.)
const int kTopResizeAdjust = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks to less than 18 px tall, plus the height of the
// frame border and any bottom edge.
const int kTitlebarMinimumHeight = 18;
// The icon is inset 2 px from the left frame border.
const int kIconLeftSpacing = 2;
// The icon takes up 16/25th of the available titlebar height.  (This is
// expressed as two ints to avoid precision losses leading to off-by-one pixel
// errors.)
const int kIconHeightFractionNumerator = 16;
const int kIconHeightFractionDenominator = 25;
// The icon never shrinks below 16 px on a side.
const int kIconMinimumSize = 16;
// Because our frame border has a different "3D look" than Windows', with a less
// cluttered top edge, we need to shift the icon up by 1 px in restored mode so
// it looks more centered.
const int kIconRestoredAdjust = 1;
// There is a 4 px gap between the icon and the title text.
const int kIconTitleSpacing = 4;
// The title text starts 2 px below the bottom of the top frame border.
const int kTitleTopSpacing = 2;
// There is a 5 px gap between the title text and the distributor logo (if
// present) or caption buttons.
const int kTitleLogoSpacing = 5;
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
// OpaqueBrowserFrameView, public:

OpaqueBrowserFrameView::OpaqueBrowserFrameView(BrowserFrame* frame,
                                               BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      minimize_button_(new views::ImageButton(this)),
      maximize_button_(new views::ImageButton(this)),
      restore_button_(new views::ImageButton(this)),
      close_button_(new views::ImageButton(this)),
      window_icon_(NULL),
      frame_(frame),
      browser_view_(browser_view) {
  InitClass();

  ThemeProvider* tp = frame_->GetThemeProviderForFrame();

  minimize_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_MINIMIZE));
  minimize_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_MINIMIZE_H));
  minimize_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_MINIMIZE_P));
  minimize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_);

  maximize_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_MAXIMIZE));
  maximize_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_MAXIMIZE_H));
  maximize_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_MAXIMIZE_P));
  maximize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);

  restore_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RESTORE));
  restore_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RESTORE_H));
  restore_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RESTORE_P));
  restore_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_RESTORE));
  AddChildView(restore_button_);

  close_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_CLOSE));
  close_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_CLOSE_H));
  close_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_CLOSE_P));
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view_->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }
  // Only load the title font if we're going to need to use it to paint.
  // Loading fonts is expensive.
  if (browser_view_->ShouldShowWindowTitle())
    InitAppWindowResources();
}

OpaqueBrowserFrameView::~OpaqueBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForTabStrip(
    TabStrip* tabstrip) const {
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_bounds_.right() + kOTRSideSpacing) :
      NonClientBorderThickness();
  int tabstrip_width = minimize_button_->x() - tabstrip_x -
      (frame_->GetWindow()->IsMaximized() ?
      kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);
  return gfx::Rect(tabstrip_x, NonClientTopBorderHeight(),
                   std::max(0, tabstrip_width), tabstrip->GetPreferredHeight());
}

void OpaqueBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

gfx::Size OpaqueBrowserFrameView::GetMinimumSize() {
  gfx::Size min_size(browser_view_->GetMinimumSize());
  int border_thickness = NonClientBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);

  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  int min_titlebar_width = (2 * FrameBorderThickness()) + kIconLeftSpacing +
    (d->ShouldShowWindowIcon() ?
        (IconSize(NULL, NULL, NULL) + kTitleLogoSpacing) : 0) +
    ((distributor_logo_ && browser_view_->ShouldShowDistributorLogo()) ?
        (distributor_logo_->width() + kLogoCaptionSpacing) : 0) +
    minimize_button_->GetMinimumSize().width() +
    restore_button_->GetMinimumSize().width() +
    close_button_->GetMinimumSize().width();
  min_size.set_width(std::max(min_size.width(), min_titlebar_width));

  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect OpaqueBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

gfx::Point OpaqueBrowserFrameView::GetSystemMenuPoint() const {
  gfx::Point system_menu_point(
      MirroredXCoordinateInsideView(FrameBorderThickness()),
      NonClientTopBorderHeight() + browser_view_->GetTabStripHeight() -
      (frame_->GetWindow()->IsFullscreen() ? 0 : kClientEdgeThickness));
  ConvertPointToScreen(this, &system_menu_point);
  return system_menu_point;
}

int OpaqueBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;
  if (restore_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMAXBUTTON;
  if (maximize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMAXBUTTON;
  if (minimize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMINBUTTON;
  if (window_icon_ &&
      window_icon_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTSYSMENU;

  int window_component = GetHTComponentForFrame(point, TopResizeHeight(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void OpaqueBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
  DCHECK(window_mask);

  if (frame_->GetWindow()->IsMaximized() || frame_->GetWindow()->IsFullscreen())
    return;

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 2);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(2, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 2), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 2);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void OpaqueBrowserFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void OpaqueBrowserFrameView::ResetWindowControls() {
  restore_button_->SetState(views::CustomButton::BS_NORMAL);
  minimize_button_->SetState(views::CustomButton::BS_NORMAL);
  maximize_button_->SetState(views::CustomButton::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::View overrides:

void OpaqueBrowserFrameView::Paint(gfx::Canvas* canvas) {
  views::Window* window = frame_->GetWindow();
  if (window->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  if (window->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);
  PaintDistributorLogo(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  PaintOTRAvatar(canvas);
  if (!window->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void OpaqueBrowserFrameView::Layout() {
  LayoutWindowControls();
  LayoutDistributorLogo();
  LayoutTitleBar();
  LayoutOTRAvatar();
  LayoutClientView();
}

bool OpaqueBrowserFrameView::HitTest(const gfx::Point& l) const {
  // If the point is outside the bounds of the client area, claim it.
  bool in_nonclient = NonClientFrameView::HitTest(l);
  if (in_nonclient)
    return in_nonclient;

  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  if (l.y() > browser_view_->tabstrip()->bounds().bottom())
    return false;

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToView would otherwise return something bogus.
  gfx::Point tabstrip_point(l);
  View::ConvertPointToView(GetParent(), browser_view_->tabstrip(),
                           &tabstrip_point);
  return browser_view_->tabstrip()->PointIsWithinWindowCaption(tabstrip_point);
}

void OpaqueBrowserFrameView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (is_add && child == this) {
    // The Accessibility glue looks for the product name on these two views to
    // determine if this is in fact a Chrome window.
    GetRootView()->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
    SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  }
}

bool OpaqueBrowserFrameView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);
  // We aren't actually the client area of the window, but we act like it as
  // far as accessibility and the UI tests are concerned.
  *role = AccessibilityTypes::ROLE_CLIENT;
  return true;
}

bool OpaqueBrowserFrameView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void OpaqueBrowserFrameView::SetAccessibleName(const std::wstring& name) {
  accessible_name_ = name;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::ButtonListener implementation:

void OpaqueBrowserFrameView::ButtonPressed(views::Button* sender) {
  views::Window* window = frame_->GetWindow();
  if (sender == minimize_button_)
    window->Minimize();
  else if (sender == maximize_button_)
    window->Maximize();
  else if (sender == restore_button_)
    window->Restore();
  else if (sender == close_button_)
    window->Close();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool OpaqueBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap OpaqueBrowserFrameView::GetFavIconForTabIconView() {
  return frame_->GetWindow()->GetDelegate()->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, private:

int OpaqueBrowserFrameView::FrameBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  return (window->IsMaximized() || window->IsFullscreen()) ?
      0 : kFrameBorderThickness;
}

int OpaqueBrowserFrameView::TopResizeHeight() const {
  return FrameBorderThickness() - kTopResizeAdjust;
}

int OpaqueBrowserFrameView::NonClientBorderThickness() const {
  // When we fill the screen, we don't show a client edge.
  views::Window* window = frame_->GetWindow();
  return FrameBorderThickness() +
      ((window->IsMaximized() || window->IsFullscreen()) ?
       0 : kClientEdgeThickness);
}

int OpaqueBrowserFrameView::NonClientTopBorderHeight() const {
  views::Window* window = frame_->GetWindow();
  if (window->GetDelegate()->ShouldShowWindowTitle())
    return TitleCoordinates(NULL, NULL);

  if (browser_view_->IsTabStripVisible() && window->IsMaximized())
    return FrameBorderThickness() - kTabstripTopShadowThickness;

  return FrameBorderThickness() +
      ((window->IsMaximized() || window->IsFullscreen()) ?
       0 : kNonClientRestoredExtraThickness);
}

int OpaqueBrowserFrameView::UnavailablePixelsAtBottomOfNonClientHeight() const {
  // Tricky: When a toolbar is edging the titlebar, it not only draws its own
  // shadow and client edge, but an extra, light "shadow" pixel as well, which
  // is treated as available space.  Thus the nonclient area actually _fails_ to
  // include some available pixels, leading to a negative number here.
  if (browser_view_->IsToolbarVisible())
    return -kFrameShadowThickness;

  return kFrameShadowThickness +
      (frame_->GetWindow()->IsMaximized() ? 0 : kClientEdgeThickness);
}

int OpaqueBrowserFrameView::TitleCoordinates(int* title_top_spacing_ptr,
                                             int* title_thickness_ptr) const {
  int frame_thickness = FrameBorderThickness();
  int min_titlebar_height = kTitlebarMinimumHeight + frame_thickness;
  int title_top_spacing = frame_thickness + kTitleTopSpacing;
  // The bottom spacing should be the same apparent height as the top spacing.
  // Because the actual top spacing height varies based on the system border
  // thickness, we calculate this based on the restored top spacing and then
  // adjust for maximized mode.  We also don't include the frame shadow here,
  // since while it's part of the bottom spacing it will be added in at the end
  // as necessary (when a toolbar is present, the "shadow" is actually drawn by
  // the toolbar).
  int title_bottom_spacing =
      kFrameBorderThickness + kTitleTopSpacing - kFrameShadowThickness;
  if (frame_->GetWindow()->IsMaximized()) {
    // When we maximize, the top border appears to be chopped off; shift the
    // title down to stay centered within the remaining space.
    int title_adjust = (kFrameBorderThickness / 2);
    title_top_spacing += title_adjust;
    title_bottom_spacing -= title_adjust;
  }
  int title_thickness = std::max(title_font_->height(),
      min_titlebar_height - title_top_spacing - title_bottom_spacing);
  if (title_top_spacing_ptr)
    *title_top_spacing_ptr = title_top_spacing;
  if (title_thickness_ptr)
    *title_thickness_ptr = title_thickness;
  return title_top_spacing + title_thickness + title_bottom_spacing +
      UnavailablePixelsAtBottomOfNonClientHeight();
}

int OpaqueBrowserFrameView::IconSize(int* title_top_spacing_ptr,
                                     int* title_thickness_ptr,
                                     int* available_height_ptr) const {
  // The usable height of the titlebar area is the total height minus the top
  // resize border and any edge area we draw at its bottom.
  int frame_thickness = FrameBorderThickness();
  int top_height = TitleCoordinates(title_top_spacing_ptr, title_thickness_ptr);
  int available_height = top_height - frame_thickness -
      UnavailablePixelsAtBottomOfNonClientHeight();
  if (available_height_ptr)
    *available_height_ptr = available_height;

  // The icon takes up a constant fraction of the available height, down to a
  // minimum size, and is always an even number of pixels on a side (presumably
  // to make scaled icons look better).  It's centered within the usable height.
  return std::max((available_height * kIconHeightFractionNumerator /
      kIconHeightFractionDenominator) / 2 * 2, kIconMinimumSize);
}

void OpaqueBrowserFrameView::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  SkBitmap* top_left_corner = tp->GetBitmapNamed(IDR_WINDOW_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      tp->GetBitmapNamed(IDR_WINDOW_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = tp->GetBitmapNamed(IDR_WINDOW_TOP_CENTER);
  SkBitmap* right_edge = tp->GetBitmapNamed(IDR_WINDOW_RIGHT_SIDE);
  SkBitmap* left_edge = tp->GetBitmapNamed(IDR_WINDOW_LEFT_SIDE);
  SkBitmap* bottom_left_corner =
      tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_CENTER);


  // Window frame mode and color
  SkBitmap* theme_frame;
  SkColor frame_color;
  if (!browser_view_->IsOffTheRecord()) {
    if (frame_->GetWindow()->IsActive()) {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME);
    } else {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME_INACTIVE);
    }
  } else {
    if (frame_->GetWindow()->IsActive()) {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME_INCOGNITO);
    } else {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO_INACTIVE);
      frame_color = tp->GetColor(
          BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE);
    }
  }

  // Fill with the frame color first so we have a constant background for
  // areas not covered by the theme image.
  canvas->FillRectInt(frame_color, 0, 0, width(), theme_frame->height());
  // Now fill down the sides
  canvas->FillRectInt(frame_color,
                      0, theme_frame->height(),
                      left_edge->width(), height() - theme_frame->height());
  canvas->FillRectInt(frame_color,
                      width() - right_edge->width(), theme_frame->height(),
                      right_edge->width(), height() - theme_frame->height());
  // Now fill the bottom area.
  canvas->FillRectInt(frame_color,
      left_edge->width(), height() - bottom_edge->height(),
      width() - left_edge->width() - right_edge->width(),
      bottom_edge->height());

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), theme_frame->height());

  // Draw the theme frame overlay
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY)) {
    SkBitmap* theme_overlay = tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY);
    canvas->DrawBitmapInt(*theme_overlay, 0, 0);
  }

  // Top.
  int top_left_height = std::min(top_left_corner->height(),
                                 height() - bottom_left_corner->height());
  canvas->DrawBitmapInt(*top_left_corner, 0, 0, top_left_corner->width(),
      top_left_height, 0, 0, top_left_corner->width(), top_left_height, false);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  int top_right_height = std::min(top_right_corner->height(),
                                  height() - bottom_right_corner->height());
  canvas->DrawBitmapInt(*top_right_corner, 0, 0, top_right_corner->width(),
      top_right_height, width() - top_right_corner->width(), 0,
      top_right_corner->width(), top_right_height, false);
  // Note: When we don't have a toolbar, we need to draw some kind of bottom
  // edge here.  Because the App Window graphics we use for this have an
  // attached client edge and their sizing algorithm is a little involved, we do
  // all this in PaintRestoredClientEdge().

  // Right.
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
      top_right_height, right_edge->width(),
      height() - top_right_height - bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width() - bottom_right_corner->width(),
                        height() - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
                       height() - bottom_edge->height(),
                       width() - bottom_left_corner->width() -
                           bottom_right_corner->width(),
                       bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height() - bottom_left_corner->height());

  // Left.
  canvas->TileImageInt(*left_edge, 0, top_left_height, left_edge->width(),
      height() - top_left_height - bottom_left_corner->height());
}


void OpaqueBrowserFrameView::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();
  views::Window* window = frame_->GetWindow();

  // Window frame mode and color
  SkBitmap* theme_frame;
  if (!browser_view_->IsOffTheRecord()) {
    theme_frame = window->IsActive() ?
        tp->GetBitmapNamed(IDR_THEME_FRAME) :
        tp->GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
  } else {
    theme_frame = window->IsActive() ?
        tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO) :
        tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO_INACTIVE);
  }

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), theme_frame->height());

  // Draw the theme frame overlay
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY)) {
    SkBitmap* theme_overlay = tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY);
    canvas->DrawBitmapInt(*theme_overlay, 0, 0);
  }

  if (!browser_view_->IsToolbarVisible()) {
    // There's no toolbar to edge the frame border, so we need to draw a bottom
    // edge.  The graphic we use for this has a built in client edge, so we clip
    // it off the bottom.
    SkBitmap* top_center =
        tp->GetBitmapNamed(IDR_APP_TOP_CENTER);
    int edge_height = top_center->height() - kClientEdgeThickness;
    canvas->TileImageInt(*top_center, 0,
        window->GetClientView()->y() - edge_height, width(), edge_height);
  }
}

void OpaqueBrowserFrameView::PaintDistributorLogo(gfx::Canvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized and
  // when we actually have a logo.
  if (!frame_->GetWindow()->IsMaximized() && distributor_logo_ &&
      browser_view_->ShouldShowDistributorLogo()) {
    canvas->DrawBitmapInt(*distributor_logo_,
        MirroredLeftPointForRect(logo_bounds_), logo_bounds_.y());
  }
}

void OpaqueBrowserFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  if (d->ShouldShowWindowTitle()) {
    canvas->DrawStringInt(d->GetWindowTitle(), *title_font_, SK_ColorWHITE,
        MirroredLeftPointForRect(title_bounds_), title_bounds_.y(),
        title_bounds_.width(), title_bounds_.height());
    /* TODO(pkasting):  If this window is active, we should also draw a drop
     * shadow on the title.  This is tricky, because we don't want to hardcode a
     * shadow color (since we want to work with various themes), but we can't
     * alpha-blend either (since the Windows text APIs don't really do this).
     * So we'd need to sample the background color at the right location and
     * synthesize a good shadow color. */
  }
}

void OpaqueBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  if (!browser_view_->IsToolbarVisible())
    return;

  ThemeProvider* tp = GetThemeProvider();
  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                           this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  int strip_height = browser_view_->GetTabStripHeight();
  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);

  canvas->TileImageInt(*theme_toolbar,
      0, strip_height - 1,  // crop src
      toolbar_bounds.x() - 1, toolbar_bounds.y() + 2,
      toolbar_bounds.width() + 2, theme_toolbar->height());

  SkBitmap* toolbar_left =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);

  // Gross hack: We split the toolbar images into two pieces, since sometimes
  // (popup mode) the toolbar isn't tall enough to show the whole image.  The
  // split happens between the top shadow section and the bottom gradient
  // section so that we never break the gradient.
  int split_point = kFrameShadowThickness * 2;
  int bottom_y = toolbar_bounds.y() + split_point;
  int bottom_edge_height =
      std::min(toolbar_left->height(), toolbar_bounds.height()) - split_point;

  canvas->DrawBitmapInt(*toolbar_left, 0, 0, toolbar_left->width(), split_point,
      toolbar_bounds.x() - toolbar_left->width(), toolbar_bounds.y(),
      toolbar_left->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_left, 0,
      toolbar_left->height() - bottom_edge_height, toolbar_left->width(),
      bottom_edge_height, toolbar_bounds.x() - toolbar_left->width(), bottom_y,
      toolbar_left->width(), bottom_edge_height, false);

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center, 0, 0, toolbar_bounds.x(),
      toolbar_bounds.y(), toolbar_bounds.width(), split_point);
  int bottom_center_height =
      std::min(toolbar_center->height(), toolbar_bounds.height()) - split_point;
  canvas->TileImageInt(*toolbar_center, 0,
      toolbar_center->height() - bottom_center_height, toolbar_bounds.x(),
      bottom_y, toolbar_bounds.width(), bottom_center_height);

  SkBitmap* toolbar_right = tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER);
  canvas->DrawBitmapInt(*toolbar_right, 0, 0, toolbar_right->width(),
      split_point, toolbar_bounds.right(), toolbar_bounds.y(),
      toolbar_right->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_right, 0,
      toolbar_right->height() - bottom_edge_height, toolbar_right->width(),
      bottom_edge_height, toolbar_bounds.right(), bottom_y,
      toolbar_right->width(), bottom_edge_height, false);

  // Draw the content/toolbar separator.
  canvas->DrawLineInt(ResourceBundle::toolbar_separator_color,
      toolbar_bounds.x(), toolbar_bounds.bottom() - 1,
      toolbar_bounds.right() - 1, toolbar_bounds.bottom() - 1);
}

void OpaqueBrowserFrameView::PaintOTRAvatar(gfx::Canvas* canvas) {
  if (!browser_view_->ShouldShowOffTheRecordAvatar())
    return;

  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  canvas->DrawBitmapInt(otr_avatar_icon, 0,
      (otr_avatar_icon.height() - otr_avatar_bounds_.height()) / 2,
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(),
      MirroredLeftPointForRect(otr_avatar_bounds_), otr_avatar_bounds_.y(),
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), false);
}

void OpaqueBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();
  int client_area_top = frame_->GetWindow()->GetClientView()->y();

  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());
  SkColor toolbar_color = tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);

  if (browser_view_->IsToolbarVisible()) {
    // The client edges start below the toolbar or its corner images, whichever
    // is shorter.
    gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
    client_area_top += browser_view_->GetToolbarBounds().y() +
        std::min(tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height(),
                 toolbar_bounds.height());
  } else {
    // The toolbar isn't going to draw a client edge for us, so draw one
    // ourselves.
    SkBitmap* top_left = tp->GetBitmapNamed(IDR_APP_TOP_LEFT);
    SkBitmap* top_center = tp->GetBitmapNamed(IDR_APP_TOP_CENTER);
    SkBitmap* top_right = tp->GetBitmapNamed(IDR_APP_TOP_RIGHT);
    int top_edge_y = client_area_top - top_center->height();
    int height = client_area_top - top_edge_y;

    canvas->DrawBitmapInt(*top_left, 0, 0, top_left->width(), height,
        client_area_bounds.x() - top_left->width(), top_edge_y,
        top_left->width(), height, false);
    canvas->TileImageInt(*top_center, 0, 0, client_area_bounds.x(), top_edge_y,
      client_area_bounds.width(), std::min(height, top_center->height()));
    canvas->DrawBitmapInt(*top_right, 0, 0, top_right->width(), height,
        client_area_bounds.right(), top_edge_y,
        top_right->width(), height, false);

    // Draw the toolbar color across the top edge.
    canvas->DrawLineInt(toolbar_color,
        client_area_bounds.x() - kClientEdgeThickness,
        client_area_top - kClientEdgeThickness,
        client_area_bounds.right() + kClientEdgeThickness,
        client_area_top - kClientEdgeThickness);
  }

  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness());
  int client_area_height = client_area_bottom - client_area_top;

  // Draw the toolbar color so that the one pixel areas down the sides
  // show the right color even if not covered by the toolbar image.
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

  SkBitmap* right = tp->GetBitmapNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);
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

void OpaqueBrowserFrameView::LayoutWindowControls() {
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                   views::ImageButton::ALIGN_BOTTOM);
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  bool is_maximized = frame_->GetWindow()->IsMaximized();
  int frame_thickness = FrameBorderThickness();
  int caption_y = is_maximized ? frame_thickness : kFrameShadowThickness;
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the rightmost
  // button to the screen corner to obey Fitts' Law.
  int right_extra_width = is_maximized ?
      (kFrameBorderThickness - kFrameShadowThickness) : 0;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - close_button_size.width() -
      right_extra_width - frame_thickness, caption_y,
      close_button_size.width() + right_extra_width,
      close_button_size.height());

  // When the window is restored, we show a maximized button; otherwise, we show
  // a restore button.
  bool is_restored = !is_maximized && !frame_->GetWindow()->IsMinimized();
  views::ImageButton* invisible_button = is_restored ?
      restore_button_ : maximize_button_;
  invisible_button->SetVisible(false);

  views::ImageButton* visible_button = is_restored ?
      maximize_button_ : restore_button_;
  visible_button->SetVisible(true);
  visible_button->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_BOTTOM);
  gfx::Size visible_button_size = visible_button->GetPreferredSize();
  visible_button->SetBounds(close_button_->x() - visible_button_size.width(),
                            caption_y, visible_button_size.width(),
                            visible_button_size.height());

  minimize_button_->SetVisible(true);
  minimize_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                      views::ImageButton::ALIGN_BOTTOM);
  gfx::Size minimize_button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      visible_button->x() - minimize_button_size.width(), caption_y,
      minimize_button_size.width(),
      minimize_button_size.height());
}

void OpaqueBrowserFrameView::LayoutDistributorLogo() {
  // Always lay out the logo, even when it's not present, so we can lay out the
  // window title based on its position.
  if (distributor_logo_ && browser_view_->ShouldShowDistributorLogo()) {
    logo_bounds_.SetRect(minimize_button_->x() - distributor_logo_->width() -
        kLogoCaptionSpacing, TopResizeHeight(), distributor_logo_->width(),
        distributor_logo_->height());
  } else {
    logo_bounds_.SetRect(minimize_button_->x(), TopResizeHeight(), 0, 0);
  }
}

void OpaqueBrowserFrameView::LayoutTitleBar() {
  // Always lay out the icon, even when it's not present, so we can lay out the
  // window title based on its position.
  int frame_thickness = FrameBorderThickness();
  int icon_x = frame_thickness + kIconLeftSpacing;

  InitAppWindowResources();  // ! Should we do this?  Isn't this a perf hit?
  int title_top_spacing, title_thickness, available_height;
  int icon_size =
      IconSize(&title_top_spacing, &title_thickness, &available_height);
  int icon_y = ((available_height - icon_size) / 2) + frame_thickness;

  // Hack: Our frame border has a different "3D look" than Windows'.  Theirs has
  // a more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered in
  // the remaining space.  Because the apparent shape of our border is simpler,
  // using the same positioning makes things look slightly uncentered with
  // restored windows, so we come up to compensate.
  if (!frame_->GetWindow()->IsMaximized())
    icon_y -= kIconRestoredAdjust;

  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  if (!d->ShouldShowWindowIcon())
    icon_size = 0;
  if (window_icon_)
    window_icon_->SetBounds(icon_x, icon_y, icon_size, icon_size);

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    int title_x = icon_x + icon_size +
        (d->ShouldShowWindowIcon() ? kIconTitleSpacing : 0);
    title_bounds_.SetRect(title_x,
        title_top_spacing + ((title_thickness - title_font_->height()) / 2),
        std::max(0, logo_bounds_.x() - kTitleLogoSpacing - title_x),
        title_font_->height());
  }
}

void OpaqueBrowserFrameView::LayoutOTRAvatar() {
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

void OpaqueBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect OpaqueBrowserFrameView::CalculateClientAreaBounds(int width,
                                                            int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

// static
void OpaqueBrowserFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
#if defined(GOOGLE_CHROME_BUILD)
    distributor_logo_ = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_DISTRIBUTOR_LOGO_LIGHT);
#endif

    initialized = true;
  }
}

// static
void OpaqueBrowserFrameView::InitAppWindowResources() {
  static bool initialized = false;
  if (!initialized) {
#if defined(OS_WIN)
    title_font_ = new gfx::Font(win_util::GetWindowTitleFont());
#else
    NOTIMPLEMENTED();
    title_font_ = new gfx::Font();
#endif
    initialized = true;
  }
}
