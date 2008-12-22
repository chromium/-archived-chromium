// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/opaque_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window_resources.h"
#include "chromium_strings.h"
#include "generated_resources.h"

// An enumeration of bitmap resources used by this window.
enum {
  FRAME_PART_BITMAP_FIRST = 0,  // Must be first.

  // Window Controls.
  FRAME_CLOSE_BUTTON_ICON,
  FRAME_CLOSE_BUTTON_ICON_H,
  FRAME_CLOSE_BUTTON_ICON_P,
  FRAME_CLOSE_BUTTON_ICON_SA,
  FRAME_CLOSE_BUTTON_ICON_SA_H,
  FRAME_CLOSE_BUTTON_ICON_SA_P,
  FRAME_RESTORE_BUTTON_ICON,
  FRAME_RESTORE_BUTTON_ICON_H,
  FRAME_RESTORE_BUTTON_ICON_P,
  FRAME_MAXIMIZE_BUTTON_ICON,
  FRAME_MAXIMIZE_BUTTON_ICON_H,
  FRAME_MAXIMIZE_BUTTON_ICON_P,
  FRAME_MINIMIZE_BUTTON_ICON,
  FRAME_MINIMIZE_BUTTON_ICON_H,
  FRAME_MINIMIZE_BUTTON_ICON_P,

  // Window Frame Border.
  FRAME_BOTTOM_EDGE,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_EDGE,
  FRAME_RIGHT_EDGE,
  FRAME_TOP_EDGE,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

  // Window Maximized Border.
  FRAME_MAXIMIZED_TOP_EDGE,
  FRAME_MAXIMIZED_BOTTOM_EDGE,

  // Client Edge Border.
  FRAME_CLIENT_EDGE_TOP_LEFT,
  FRAME_CLIENT_EDGE_TOP,
  FRAME_CLIENT_EDGE_TOP_RIGHT,
  FRAME_CLIENT_EDGE_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM,
  FRAME_CLIENT_EDGE_BOTTOM_LEFT,
  FRAME_CLIENT_EDGE_LEFT,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

class ActiveWindowResources : public views::WindowResources {
 public:
  ActiveWindowResources() {
    InitClass();
  }
  virtual ~ActiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_WINDOW_BOTTOM_CENTER, IDR_WINDOW_BOTTOM_LEFT_CORNER,
            IDR_WINDOW_BOTTOM_RIGHT_CORNER, IDR_WINDOW_LEFT_SIDE,
            IDR_WINDOW_RIGHT_SIDE, IDR_WINDOW_TOP_CENTER,
            IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_RIGHT_CORNER,
        IDR_WINDOW_TOP_CENTER, IDR_WINDOW_BOTTOM_CENTER,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ActiveWindowResources);
};

class InactiveWindowResources : public views::WindowResources {
 public:
  InactiveWindowResources() {
    InitClass();
  }
  virtual ~InactiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_DEWINDOW_BOTTOM_CENTER, IDR_DEWINDOW_BOTTOM_LEFT_CORNER,
            IDR_DEWINDOW_BOTTOM_RIGHT_CORNER, IDR_DEWINDOW_LEFT_SIDE,
            IDR_DEWINDOW_RIGHT_SIDE, IDR_DEWINDOW_TOP_CENTER,
            IDR_DEWINDOW_TOP_LEFT_CORNER, IDR_DEWINDOW_TOP_RIGHT_CORNER,
        IDR_DEWINDOW_TOP_CENTER, IDR_DEWINDOW_BOTTOM_CENTER,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(InactiveWindowResources);
};

class OTRActiveWindowResources : public views::WindowResources {
 public:
  OTRActiveWindowResources() {
    InitClass();
  }
  virtual ~OTRActiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  
 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_WINDOW_BOTTOM_CENTER_OTR, IDR_WINDOW_BOTTOM_LEFT_CORNER_OTR,
            IDR_WINDOW_BOTTOM_RIGHT_CORNER_OTR, IDR_WINDOW_LEFT_SIDE_OTR,
            IDR_WINDOW_RIGHT_SIDE_OTR, IDR_WINDOW_TOP_CENTER_OTR,
            IDR_WINDOW_TOP_LEFT_CORNER_OTR, IDR_WINDOW_TOP_RIGHT_CORNER_OTR,
        IDR_WINDOW_TOP_CENTER_OTR, IDR_WINDOW_BOTTOM_CENTER_OTR,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(OTRActiveWindowResources);
};

class OTRInactiveWindowResources : public views::WindowResources {
 public:
  OTRInactiveWindowResources() {
    InitClass();
  }
  virtual ~OTRInactiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_DEWINDOW_BOTTOM_CENTER_OTR, IDR_DEWINDOW_BOTTOM_LEFT_CORNER_OTR,
            IDR_DEWINDOW_BOTTOM_RIGHT_CORNER_OTR, IDR_DEWINDOW_LEFT_SIDE_OTR,
            IDR_DEWINDOW_RIGHT_SIDE_OTR, IDR_DEWINDOW_TOP_CENTER_OTR,
            IDR_DEWINDOW_TOP_LEFT_CORNER_OTR,
            IDR_DEWINDOW_TOP_RIGHT_CORNER_OTR,
        IDR_DEWINDOW_TOP_CENTER_OTR, IDR_DEWINDOW_BOTTOM_CENTER_OTR,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(OTRInactiveWindowResources);
};
// static
SkBitmap* ActiveWindowResources::standard_frame_bitmaps_[];
SkBitmap* InactiveWindowResources::standard_frame_bitmaps_[];
SkBitmap* OTRActiveWindowResources::standard_frame_bitmaps_[];
SkBitmap* OTRInactiveWindowResources::standard_frame_bitmaps_[];

views::WindowResources* OpaqueNonClientView::active_resources_ = NULL;
views::WindowResources* OpaqueNonClientView::inactive_resources_ = NULL;
views::WindowResources* OpaqueNonClientView::active_otr_resources_ = NULL;
views::WindowResources* OpaqueNonClientView::inactive_otr_resources_ = NULL;
SkBitmap OpaqueNonClientView::distributor_logo_;
SkBitmap OpaqueNonClientView::app_top_left_;
SkBitmap OpaqueNonClientView::app_top_center_;
SkBitmap OpaqueNonClientView::app_top_right_;
ChromeFont OpaqueNonClientView::title_font_;

// The distance between the top of the window and the top of the window
// controls when the window is restored.
static const int kWindowControlsTopOffset = 1;
// The distance between the right edge of the window and the right edge of the
// right-most window control when the window is restored.
static const int kWindowControlsRightOffset = 4;
// The distance between the top of the window and the top of the window
// controls' images when the window is maximized.  We extend the clickable area
// all the way to the top of the window to obey Fitts' Law.
static const int kWindowControlsTopZoomedExtraHeight = 5;
// The distance between the right edge of the window and the right edge of the
// right-most window control when the window is maximized.
static const int kWindowControlsRightZoomedOffset = 7;
// The distance between the left edge of the window and the left edge of the
// window icon when a title-bar is showing.
static const int kWindowIconLeftOffset = 5;
// The distance between the left edge of the window and the left edge of the
// window icon when a title-bar is showing and the window is maximized.
static const int kWindowIconLeftZoomedOffset = 6;
// The distance between the top edge of the window and the top edge of the
// window icon when a title-bar is showing.
static const int kWindowIconTopOffset = 6;
// The distance between the top edge of the window and the top edge of the
// window icon when a title-bar is showing and the window is maximized.
static const int kWindowIconTopZoomedOffset = 8;
// The distance between the window icon and the window title when a title-bar
// is showing.
static const int kWindowIconTitleSpacing = 4;
// The distance between the top of the window and the title text when a
// title-bar is showing.
static const int kTitleTopOffset = 6;
// The distance between the top of the window and the title text when a
// title-bar is showing and the window is maximized.
static const int kTitleTopZoomedOffset = 8;
// The distance between the right edge of the title text bounding box and the
// left edge of the distributor logo.
static const int kTitleLogoSpacing = 5;
// The distance between the bottom of the title text and the TabStrip when a
// title-bar is showing.
static const int kTitleBottomSpacing = 6;
// The distance between the top edge of the window and the TabStrip when there
// is no title-bar showing, and the window is restored.
static const int kNoTitleTopSpacing = 15;
// The distance between the top edge of the window and the TabStrip when there
// is no title-bar showing, and the window is maximized.
static const int kNoTitleZoomedTopSpacing = 4;
// The number of pixels to shave off the bottom of the images making up the top
// client edge when the window is maximized, so we only draw a shadowed titlebar
// and not a grey client area border below it.
static const int kClientEdgeZoomedOffset = 1;
// The amount of horizontal and vertical distance from a corner of the window
// within which a mouse-drive resize operation will resize the window in two
// dimensions.
static const int kResizeAreaCornerSize = 16;
// The width of the sizing border on the left and right edge of the window.
static const int kWindowHorizontalBorderSize = 5;
// The width of the sizing border on the left and right edge of the window when
// the window is maximized.
static const int kWindowHorizontalBorderZoomedSize = 4;
// The height of the sizing border at the top edge of the window
static const int kWindowVerticalBorderTopSize = 3;
// The height of the sizing border on the bottom edge of the window.
static const int kWindowVerticalBorderBottomSize = 5;
// The distance between the bottom edge of the window image and the true bottom
// edge of the window when the window is maximized.
static const int kWindowBottomZoomedSpacing = 4;
// The width and height of the window icon that appears at the top left of
// pop-up and app windows.
static const int kWindowIconSize = 16;
// The horizontal distance of the right edge of the distributor logo from the
// left edge of the left-most window control.
static const int kDistributorLogoHorizontalOffset = 7;
// The vertical distance of the top of the distributor logo from the top edge
// of the window.
static const int kDistributorLogoVerticalOffset = 3;
// The distance from the left of the window of the OTR avatar icon.
static const int kOTRAvatarIconMargin = 9;
// The distance from the top of the window of the OTR avatar icon when the
// window is maximized.
static const int kNoTitleOTRZoomedTopSpacing = 3;
// Horizontal distance between the right edge of the new tab icon and the left
// edge of the window minimize icon when the window is maximized.
static const int kNewTabIconWindowControlsSpacing = 10;

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, public:

OpaqueNonClientView::OpaqueNonClientView(OpaqueFrame* frame,
                                         BrowserView* browser_view)
    : NonClientView(),
      minimize_button_(new views::Button),
      maximize_button_(new views::Button),
      restore_button_(new views::Button),
      close_button_(new views::Button),
      window_icon_(NULL),
      frame_(frame),
      browser_view_(browser_view) {
  InitClass();
  if (browser_view->IsOffTheRecord()) {
    if (!active_otr_resources_) {
      // Lazy load OTR resources only when we first show an OTR frame.
      active_otr_resources_ = new OTRActiveWindowResources;
      inactive_otr_resources_ = new OTRInactiveWindowResources;
    }
    current_active_resources_ = active_otr_resources_;
    current_inactive_resources_= inactive_otr_resources_;
  } else {
    current_active_resources_ = active_resources_;
    current_inactive_resources_ = inactive_resources_;
  }

  views::WindowResources* resources = current_active_resources_;
  minimize_button_->SetImage(
      views::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON));
  minimize_button_->SetImage(
      views::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_H));
  minimize_button_->SetImage(
      views::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_P));
  minimize_button_->SetListener(this, -1);
  minimize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_);

  maximize_button_->SetImage(
      views::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON));
  maximize_button_->SetImage(
      views::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_H));
  maximize_button_->SetImage(
      views::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_P));
  maximize_button_->SetListener(this, -1);
  maximize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);

  restore_button_->SetImage(
      views::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON));
  restore_button_->SetImage(
      views::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_H));
  restore_button_->SetImage(
      views::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_P));
  restore_button_->SetListener(this, -1);
  restore_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_RESTORE));
  AddChildView(restore_button_);

  close_button_->SetImage(
      views::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(
      views::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(
      views::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetListener(this, -1);
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

OpaqueNonClientView::~OpaqueNonClientView() {
}

gfx::Rect OpaqueNonClientView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) {
  int top_height = CalculateNonClientTopHeight();
  int horizontal_border = frame_->IsMaximized() ?
      kWindowHorizontalBorderZoomedSize : kWindowHorizontalBorderSize;
  int window_x = std::max(0, client_bounds.x() - horizontal_border);
  int window_y = std::max(0, client_bounds.y() - top_height);
  int window_w = client_bounds.width() + (2 * horizontal_border);
  int window_h =
      client_bounds.height() + top_height + kWindowVerticalBorderBottomSize;
  return gfx::Rect(window_x, window_y, window_w, window_h);
}

gfx::Rect OpaqueNonClientView::GetBoundsForTabStrip(TabStrip* tabstrip) {
  int tabstrip_height = tabstrip->GetPreferredHeight();
  int tabstrip_x = otr_avatar_bounds_.right();
  int tabstrip_width = minimize_button_->x() - tabstrip_x;
  if (frame_->IsMaximized())
    tabstrip_width -= kNewTabIconWindowControlsSpacing;
  return gfx::Rect(tabstrip_x, 0, std::max(0, tabstrip_width),
                   tabstrip_height);
}

void OpaqueNonClientView::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->Update();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, TabIconView::TabContentsProvider implementation:

bool OpaqueNonClientView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap OpaqueNonClientView::GetFavIconForTabIconView() {
  return frame_->window_delegate()->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, views::BaseButton::ButtonListener implementation:

void OpaqueNonClientView::ButtonPressed(views::BaseButton* sender) {
  if (sender == minimize_button_) {
    frame_->ExecuteSystemMenuCommand(SC_MINIMIZE);
  } else if (sender == maximize_button_) {
    frame_->ExecuteSystemMenuCommand(SC_MAXIMIZE);
  } else if (sender == restore_button_) {
    frame_->ExecuteSystemMenuCommand(SC_RESTORE);
  } else if (sender == close_button_) {
    frame_->ExecuteSystemMenuCommand(SC_CLOSE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, views::NonClientView implementation:

gfx::Rect OpaqueNonClientView::CalculateClientAreaBounds(int width,
                                                         int height) const {
  int top_margin = CalculateNonClientTopHeight();
  int horizontal_border = frame_->IsMaximized() ?
      kWindowHorizontalBorderZoomedSize : kWindowHorizontalBorderSize;
  return gfx::Rect(horizontal_border, top_margin,
      std::max(0, width - (2 * horizontal_border)),
      std::max(0, height - top_margin - kWindowVerticalBorderBottomSize));
}

gfx::Size OpaqueNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int top_margin = CalculateNonClientTopHeight();
  int horizontal_border = frame_->IsMaximized() ?
      kWindowHorizontalBorderZoomedSize : kWindowHorizontalBorderSize;
  return gfx::Size(width + (2 * horizontal_border),
                   height + top_margin + kWindowVerticalBorderBottomSize);
}

CPoint OpaqueNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(icon_bounds_.x(), icon_bounds_.bottom());
  MapWindowPoints(frame_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int OpaqueNonClientView::NonClientHitTest(const gfx::Point& point) {
  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = frame_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  // Then see if the point is within any of the window controls.
  gfx::Rect button_bounds =
      close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTCLOSE;
  button_bounds = restore_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMAXBUTTON;
  button_bounds = maximize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMAXBUTTON;
  button_bounds = minimize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMINBUTTON;
  if (window_icon_) {
    button_bounds = window_icon_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
    if (button_bounds.Contains(point))
      return HTSYSMENU;
  }

  int horizontal_border = frame_->IsMaximized() ?
      kWindowHorizontalBorderZoomedSize : kWindowHorizontalBorderSize;
  component = GetHTComponentForFrame(
      point,
      horizontal_border,
      kResizeAreaCornerSize,
      kWindowVerticalBorderTopSize,
      frame_->window_delegate()->CanResize());
  if (component == HTNOWHERE) {
    // Finally fall back to the caption.
    if (bounds().Contains(point))
      component = HTCAPTION;
    // Otherwise, the point is outside the window's bounds.
  }
  return component;
}

void OpaqueNonClientView::GetWindowMask(const gfx::Size& size,
                                        gfx::Path* window_mask) {
  DCHECK(window_mask);

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 3);
  window_mask->lineTo(SkIntToScalar(size.width() - 0), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void OpaqueNonClientView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void OpaqueNonClientView::ResetWindowControls() {
  restore_button_->SetState(views::Button::BS_NORMAL);
  minimize_button_->SetState(views::Button::BS_NORMAL);
  maximize_button_->SetState(views::Button::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, views::View overrides:

void OpaqueNonClientView::Paint(ChromeCanvas* canvas) {
  // Clip the content area out of the rendering.
  gfx::Rect contents_bounds = browser_view_->GetClientAreaBounds();
  SkRect clip;
  clip.set(SkIntToScalar(contents_bounds.x()),
           SkIntToScalar(contents_bounds.y()),
           SkIntToScalar(contents_bounds.right()),
           SkIntToScalar(contents_bounds.bottom()));
  canvas->clipRect(clip, SkRegion::kDifference_Op);

  // Render the remaining portions of the non-client area.
  if (frame_->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintFrameBorder(canvas);
  PaintOTRAvatar(canvas);
  PaintDistributorLogo(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  PaintClientEdge(canvas);
}

void OpaqueNonClientView::Layout() {
  LayoutWindowControls();
  LayoutOTRAvatar();
  LayoutDistributorLogo();
  LayoutTitleBar();
  LayoutClientView();
}

gfx::Size OpaqueNonClientView::GetPreferredSize() {
  int horizontal_border = frame_->IsMaximized() ?
      kWindowHorizontalBorderZoomedSize : kWindowHorizontalBorderSize;
  gfx::Size prefsize = frame_->client_view()->GetPreferredSize();
  prefsize.Enlarge(2 * horizontal_border,
                   CalculateNonClientTopHeight() +
                       kWindowVerticalBorderBottomSize);
  return prefsize;
}

views::View* OpaqueNonClientView::GetViewForPoint(const gfx::Point& point,
                                                  bool can_create_floating) {
  // We override this function because the ClientView can overlap the non -
  // client view, making it impossible to click on the window controls. We need
  // to ensure the window controls are checked _first_.
  views::View* views[] = { close_button_, restore_button_, maximize_button_,
                           minimize_button_ };
  for (int i = 0; i < arraysize(views); ++i) {
    if (!views[i]->IsVisible())
      continue;
    // Apply mirroring transformation on view bounds for RTL chrome.
    if (views[i]->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
      return views[i];
  }
  return View::GetViewForPoint(point, can_create_floating);
}

void OpaqueNonClientView::ViewHierarchyChanged(bool is_add,
                                               views::View* parent,
                                               views::View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    DCHECK(frame_->client_view()->GetParent() != this);
    AddChildView(frame_->client_view());

    // The Accessibility glue looks for the product name on these two views to
    // determine if this is in fact a Chrome window.
    GetRootView()->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
    SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  }
}

bool OpaqueNonClientView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);
  // We aren't actually the client area of the window, but we act like it as
  // far as MSAA and the UI tests are concerned.
  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_CLIENT;
  return true;
}

bool OpaqueNonClientView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void OpaqueNonClientView::SetAccessibleName(const std::wstring& name) {
  accessible_name_ = name;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, private:

void OpaqueNonClientView::SetWindowIcon(SkBitmap window_icon) {

}

int OpaqueNonClientView::CalculateNonClientTopHeight() const {
  if (frame_->window_delegate()->ShouldShowWindowTitle())
    return kTitleTopOffset + title_font_.height() + kTitleBottomSpacing;
  return frame_->IsMaximized() ? kNoTitleZoomedTopSpacing : kNoTitleTopSpacing;
}

void OpaqueNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_left_corner =
      resources()->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      resources()->GetPartBitmap(FRAME_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* right_edge = resources()->GetPartBitmap(FRAME_RIGHT_EDGE);
  SkBitmap* left_edge = resources()->GetPartBitmap(FRAME_LEFT_EDGE);
  SkBitmap* bottom_left_corner =
      resources()->GetPartBitmap(FRAME_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
    resources()->GetPartBitmap(FRAME_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = resources()->GetPartBitmap(FRAME_BOTTOM_EDGE);

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width() - top_right_corner->width(), 0);

  // Right.
  int top_stack_height = top_right_corner->height();
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
                       top_stack_height, right_edge->width(),
                       height() - top_stack_height -
                           bottom_right_corner->height());

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
  top_stack_height = top_left_corner->height();
  canvas->TileImageInt(*left_edge, 0, top_stack_height, left_edge->width(),
                       height() - top_stack_height -
                           bottom_left_corner->height());
}

void OpaqueNonClientView::PaintMaximizedFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_MAXIMIZED_TOP_EDGE);
  canvas->TileImageInt(*top_edge, 0, kNoTitleZoomedTopSpacing, width(),
                       top_edge->height());

  SkBitmap* bottom_edge =
      resources()->GetPartBitmap(FRAME_MAXIMIZED_BOTTOM_EDGE);
  canvas->TileImageInt(*bottom_edge, 0,
      height() - bottom_edge->height() - kWindowBottomZoomedSpacing, width(),
      bottom_edge->height());
}

void OpaqueNonClientView::PaintOTRAvatar(ChromeCanvas* canvas) {
  if (browser_view_->ShouldShowOffTheRecordAvatar()) {
    int icon_x = MirroredLeftPointForRect(otr_avatar_bounds_);
    canvas->DrawBitmapInt(browser_view_->GetOTRAvatarIcon(), icon_x,
                          otr_avatar_bounds_.y());
  }
}

void OpaqueNonClientView::PaintDistributorLogo(ChromeCanvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized and
  // when we actually have a logo.
  if (!frame_->IsMaximized() && !distributor_logo_.empty()) {
    int logo_x = MirroredLeftPointForRect(logo_bounds_);
    canvas->DrawBitmapInt(distributor_logo_, logo_x, logo_bounds_.y());
  }
}

void OpaqueNonClientView::PaintTitleBar(ChromeCanvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WindowDelegate* d = frame_->window_delegate();
  if (d->ShouldShowWindowTitle()) {
    int title_x = MirroredLeftPointForRect(title_bounds_);
    canvas->DrawStringInt(d->GetWindowTitle(), title_font_, SK_ColorWHITE,
                          title_x, title_bounds_.y(),
                          title_bounds_.width(), title_bounds_.height());
  }
}

void OpaqueNonClientView::PaintToolbarBackground(ChromeCanvas* canvas) {
  if (browser_view_->IsToolbarVisible() ||
      browser_view_->IsTabStripVisible()) {
    SkBitmap* toolbar_left =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
    SkBitmap* toolbar_center =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
    SkBitmap* toolbar_right =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);

    gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
    gfx::Point topleft(toolbar_bounds.x(), toolbar_bounds.y());
    View::ConvertPointToView(frame_->client_view(), this, &topleft);
    toolbar_bounds.set_x(topleft.x());
    toolbar_bounds.set_y(topleft.y());

    canvas->DrawBitmapInt(*toolbar_left,
                          toolbar_bounds.x() - toolbar_left->width(),
                          toolbar_bounds.y());
    canvas->TileImageInt(*toolbar_center,
                         toolbar_bounds.x(), toolbar_bounds.y(),
                         toolbar_bounds.width(), toolbar_center->height());
    canvas->DrawBitmapInt(*toolbar_right, toolbar_bounds.right(),
                          toolbar_bounds.y());
  }
}

void OpaqueNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  // The toolbar draws a client edge along its own bottom edge when it's
  // visible.  However, it only draws this for the width of the actual client
  // area, leaving a gap at the left and right edges:
  //
  // |             Toolbar             | <-- part of toolbar
  //  ----- (toolbar client edge) -----  <-- gap
  // |           Client area           | <-- right client edge
  //
  // To address this, we extend the left and right client edges up one pixel to
  // fill the gap, by pretending the toolbar is one pixel shorter than it really
  // is.
  //
  // Notes:
  // * This isn't strictly necessary in maximized mode, where the left and right
  //   edges aren't drawn, but it's simpler to not bother checking that.
  // * We can get away with this hackery because we only draw a top edge when
  //   there is no toolbar.  If we tried to draw a top edge over the toolbar's
  //   top edge, we'd need a different solution.
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  if (browser_view_->IsToolbarVisible())
    toolbar_bounds.set_height(std::max(0, toolbar_bounds.height() - 1));
  int client_area_top = frame_->client_view()->y() + toolbar_bounds.bottom();

  // When we don't have a toolbar to draw a top edge for us, draw a top edge.
  gfx::Rect client_area_bounds = browser_view_->GetClientAreaBounds();
  if (!browser_view_->IsToolbarVisible()) {
    // This hack is necessary because the top center bitmap is shorter than the
    // top left and right bitmaps.  We need their top edges to line up, and we
    // need the left and right edges to start below the corners' bottoms.
    // TODO(pkasting): If we just make the bitmaps the same height, a la the
    // bottom corners/center, we can remove this hack.
    int top_edge_y = client_area_top - app_top_center_.height() +
        (frame_->IsMaximized() ? kClientEdgeZoomedOffset : 0);
    client_area_top = top_edge_y + app_top_left_.height();
    canvas->DrawBitmapInt(app_top_left_,
                          client_area_bounds.x() - app_top_left_.width(),
                          top_edge_y);
    canvas->TileImageInt(app_top_center_, client_area_bounds.x(), top_edge_y,
                         client_area_bounds.width(), app_top_center_.height());
    canvas->DrawBitmapInt(app_top_right_, client_area_bounds.right(),
                          top_edge_y);
  }

  // In maximized mode, we don't need side or bottom client edges.
  if (frame_->IsMaximized())
    return;

  int client_area_bottom =
      std::max(client_area_top, height() - kWindowVerticalBorderBottomSize);
  int client_area_height = client_area_bottom - client_area_top;
  SkBitmap* right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);

  canvas->DrawBitmapInt(
      *resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT),
      client_area_bounds.right(), client_area_bottom);

  SkBitmap* bottom = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());

  SkBitmap* bottom_left =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  SkBitmap* left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);
}

void OpaqueNonClientView::LayoutWindowControls() {
  // TODO(pkasting): This function is almost identical to
  // DefaultNonClientView::LayoutWindowControls(), they should be combined.
  close_button_->SetImageAlignment(views::Button::ALIGN_LEFT,
                                   views::Button::ALIGN_BOTTOM);
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  bool is_maximized = frame_->IsMaximized();
  int top_offset = is_maximized ? 0 : kWindowControlsTopOffset;
  int top_extra_height = is_maximized ? kWindowControlsTopZoomedExtraHeight : 0;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      (width() - close_button_size.width() - (is_maximized ?
          kWindowControlsRightZoomedOffset : kWindowControlsRightOffset)),
      top_offset,
      (is_maximized ?
          // We extend the maximized close button to the screen corner to obey
          // Fitts' Law.
          (close_button_size.width() + kWindowControlsRightZoomedOffset) :
          close_button_size.width()),
      (close_button_size.height() + top_extra_height));

  // When the window is restored, we show a maximized button; otherwise, we show
  // a restore button.
  bool is_restored = !is_maximized && !frame_->IsMinimized();
  views::Button* invisible_button = is_restored ?
      restore_button_ : maximize_button_;
  invisible_button->SetVisible(false);

  views::Button* visible_button = is_restored ?
      maximize_button_ : restore_button_;
  visible_button->SetVisible(true);
  visible_button->SetImageAlignment(views::Button::ALIGN_LEFT,
                                    views::Button::ALIGN_BOTTOM);
  gfx::Size visible_button_size = visible_button->GetPreferredSize();
  visible_button->SetBounds(close_button_->x() - visible_button_size.width(),
                            top_offset,
                            visible_button_size.width(),
                            visible_button_size.height() + top_extra_height);

  minimize_button_->SetVisible(true);
  minimize_button_->SetImageAlignment(views::Button::ALIGN_LEFT,
                                      views::Button::ALIGN_BOTTOM);
  gfx::Size minimize_button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      visible_button->x() - minimize_button_size.width(),
      top_offset,
      minimize_button_size.width(),
      minimize_button_size.height() + top_extra_height);
}

void OpaqueNonClientView::LayoutOTRAvatar() {
  int otr_x = 0;
  int top_spacing =
      frame_->IsMaximized() ? kNoTitleOTRZoomedTopSpacing : kNoTitleTopSpacing;
  int otr_y = browser_view_->GetTabStripHeight() + top_spacing;
  int otr_width = 0;
  int otr_height = 0;
  if (browser_view_->ShouldShowOffTheRecordAvatar()) {
    SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
    otr_width = otr_avatar_icon.width();
    otr_height = otr_avatar_icon.height();
    otr_x = kOTRAvatarIconMargin;
    otr_y -= otr_avatar_icon.height() + 2;
  }
  otr_avatar_bounds_.SetRect(otr_x, otr_y, otr_width, otr_height);
}

void OpaqueNonClientView::LayoutDistributorLogo() {
  int logo_w = distributor_logo_.empty() ? 0 : distributor_logo_.width();
  int logo_h = distributor_logo_.empty() ? 0 : distributor_logo_.height();

  int logo_x =
      minimize_button_->x() - logo_w - kDistributorLogoHorizontalOffset;
  logo_bounds_.SetRect(logo_x, kDistributorLogoVerticalOffset, logo_w, logo_h);
}

void OpaqueNonClientView::LayoutTitleBar() {
  // Size the window icon, even if it is hidden so we can size the title based
  // on its position.
  int left_offset = frame_->IsMaximized() ?
      kWindowIconLeftZoomedOffset : kWindowIconLeftOffset;
  int top_offset = frame_->IsMaximized() ?
      kWindowIconTopZoomedOffset : kWindowIconTopOffset;
  views::WindowDelegate* d = frame_->window_delegate();
  int icon_size = d->ShouldShowWindowIcon() ? kWindowIconSize : 0;
  icon_bounds_.SetRect(left_offset, top_offset, icon_size, icon_size);
  if (window_icon_)
    window_icon_->SetBounds(icon_bounds_);

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    int spacing = d->ShouldShowWindowIcon() ? kWindowIconTitleSpacing : 0;
    int title_right = logo_bounds_.x() - kTitleLogoSpacing;
    int icon_right = icon_bounds_.right();
    int title_left = icon_right + spacing;
    int top_offset = frame_->IsMaximized() ?
        kTitleTopZoomedOffset : kTitleTopOffset;
    title_bounds_.SetRect(title_left, top_offset,
        std::max(0, title_right - icon_right), title_font_.height());
  }
}

void OpaqueNonClientView::LayoutClientView() {
  frame_->client_view()->SetBounds(CalculateClientAreaBounds(width(),
                                                             height()));
}

// static
void OpaqueNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    active_resources_ = new ActiveWindowResources;
    inactive_resources_ = new InactiveWindowResources;

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
#if defined(GOOGLE_CHROME_BUILD)
    distributor_logo_ = *rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO_LIGHT);
#endif

    app_top_left_ = *rb.GetBitmapNamed(IDR_APP_TOP_LEFT);
    app_top_center_ = *rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
    app_top_right_ = *rb.GetBitmapNamed(IDR_APP_TOP_RIGHT);      

    initialized = true;
  }
}

// static
void OpaqueNonClientView::InitAppWindowResources() {
  static bool initialized = false;
  if (!initialized) {
    title_font_ = win_util::GetWindowTitleFont();
    initialized = true;
  }
}
