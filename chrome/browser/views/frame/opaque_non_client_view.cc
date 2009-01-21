// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/opaque_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
// The distance between the bottom of the window's top border and the top of the
// window controls' images when the window is maximized.  We extend the
// clickable area all the way to the top of the window to obey Fitts' Law.
static const int kWindowControlsZoomedTopExtraHeight = 1;
// The distance between right edge of the right-most window control and the left
// edge of the window's right border when the window is maximized.
static const int kWindowControlsZoomedRightOffset = 3;
// The distance between the left edge of the window and the left edge of the
// window icon when a title-bar is showing and the window is restored.
static const int kWindowIconLeftOffset = 6;
// The distance between the right edge of the window's left border and the left
// edge of the window icon when a title-bar is showing and the window is
// maximized.
static const int kWindowIconZoomedLeftOffset = 2;
// The proportion of vertical space the icon takes up after subtracting the top
// and bottom borders of the titlebar (expressed as two values to avoid
// floating point representation errors that goof up the calculation).
static const int kWindowIconFractionNumerator = 16;
static const int kWindowIconFractionDenominator = 25;
// The distance between the top edge of the window and the top edge of the
// window title when a title-bar is showing and the window is restored.
static const int kWindowTitleTopOffset = 6;
// The distance between the bottom edge of the window's top border and the top
// edge of the window title when a title-bar is showing and the window is
// maximized.
static const int kWindowTitleZoomedTopOffset = 4;
// The distance between the window icon and the window title when a title-bar
// is showing.
static const int kWindowIconTitleSpacing = 4;
// The distance between the right edge of the title text bounding box and the
// left edge of the distributor logo.
static const int kTitleLogoSpacing = 5;
// The distance between the bottom of the title text and the TabStrip when a
// title-bar is showing and the window is restored.
static const int kTitleBottomSpacing = 7;
// The distance between the bottom of the title text and the TabStrip when a
// title-bar is showing and the window is maximized.
static const int kTitleZoomedBottomSpacing = 5;
// The distance between the top edge of the window and the TabStrip when there
// is no title-bar showing, and the window is restored.
static const int kNoTitleTopSpacing = 15;
// The number of pixels to crop off the bottom of the images making up the top
// client edge when the window is maximized, so we only draw a shadowed titlebar
// and not a grey client area border below it.
static const int kClientEdgeZoomedBottomCrop = 1;
// The amount of horizontal and vertical distance from a corner of the window
// within which a mouse-drive resize operation will resize the window in two
// dimensions.
static const int kResizeAreaCornerSize = 16;
// The width of the sizing border on the left and right edge of the window.
static const int kWindowHorizontalBorderSize = 5;
// The height of the sizing border at the top edge of the window
static const int kWindowVerticalBorderTopSize = 3;
// The height of the sizing border on the bottom edge of the window when the
// window is restored.
static const int kWindowVerticalBorderBottomSize = 5;
// The additional height beyond the system-provided thickness of the border on
// the bottom edge of the window when the window is maximized.
static const int kWindowVerticalBorderZoomedBottomSize = 1;
// The minimum width and height of the window icon that appears at the top left
// of pop-up and app windows.
static const int kWindowIconMinimumSize = 16;
// The minimum height reserved for the window title font.
static const int kWindowTitleMinimumHeight = 11;
// The horizontal distance of the right edge of the distributor logo from the
// left edge of the left-most window control.
static const int kDistributorLogoHorizontalOffset = 7;
// The vertical distance of the top of the distributor logo from the top edge
// of the window.
static const int kDistributorLogoVerticalOffset = 3;
// The distance between the left edge of the window and the OTR avatar icon when
// the window is restored.
static const int kOTRLeftOffset = 7;
// The distance between the right edge of the window's left border and the OTR
// avatar icon when the window is maximized.
static const int kOTRZoomedLeftOffset = 2;
// The distance between the top edge of the client area and the OTR avatar icon
// when the window is maximized.
static const int kOTRZoomedTopSpacing = 2;
// The distance between the bottom of the OTR avatar icon and the bottom of the
// tabstrip.
static const int kOTRBottomSpacing = 2;
// The number of pixels to crop off the top of the OTR image when the window is
// maximized.
static const int kOTRZoomedTopCrop = 4;
// Horizontal distance between the right edge of the OTR avatar icon and the
// left edge of the tabstrip.
static const int kOTRTabStripSpacing = 2;
// Horizontal distance between the right edge of the new tab icon and the left
// edge of the window minimize icon when the window is restored.
static const int kNewTabIconMinimizeSpacing = 5;
// Horizontal distance between the right edge of the new tab icon and the left
// edge of the window minimize icon when the window is maximized.
static const int kNewTabIconMinimizeZoomedSpacing = 16;

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
  int horizontal_border = HorizontalBorderSize();
  int window_x = std::max(0, client_bounds.x() - horizontal_border);
  int window_y = std::max(0, client_bounds.y() - top_height);
  int window_w = client_bounds.width() + (2 * horizontal_border);
  int window_h =
      client_bounds.height() + top_height + VerticalBorderBottomSize();
  return gfx::Rect(window_x, window_y, window_w, window_h);
}

gfx::Rect OpaqueNonClientView::GetBoundsForTabStrip(TabStrip* tabstrip) {
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_bounds_.right() + kOTRTabStripSpacing) :
      HorizontalBorderSize();
  int tabstrip_width = minimize_button_->x() - tabstrip_x -
      (frame_->IsMaximized() ?
      kNewTabIconMinimizeZoomedSpacing : kNewTabIconMinimizeSpacing);
  return gfx::Rect(tabstrip_x, CalculateNonClientTopHeight(),
                   std::max(0, tabstrip_width), tabstrip->GetPreferredHeight());
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
  int horizontal_border = HorizontalBorderSize();
  return gfx::Rect(horizontal_border, top_margin,
      std::max(0, width - (2 * horizontal_border)),
      std::max(0, height - top_margin - VerticalBorderBottomSize()));
}

gfx::Size OpaqueNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  return gfx::Size(width + (2 * HorizontalBorderSize()),
      height + CalculateNonClientTopHeight() + VerticalBorderBottomSize());
}

CPoint OpaqueNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(icon_bounds_.x(), icon_bounds_.bottom());
  MapWindowPoints(frame_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int OpaqueNonClientView::NonClientHitTest(const gfx::Point& point) {
  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int frame_component = frame_->client_view()->NonClientHitTest(point);
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

  int window_component = GetHTComponentForFrame(point, HorizontalBorderSize(),
      kResizeAreaCornerSize, kWindowVerticalBorderTopSize,
      frame_->window_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  if ((window_component == HTNOWHERE) && bounds().Contains(point))
    window_component = HTCAPTION;
  return window_component;
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
  if (frame_->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintFrameBorder(canvas);
  PaintDistributorLogo(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  PaintOTRAvatar(canvas);
  if (frame_->IsMaximized())
    PaintMaximizedClientEdge(canvas);
  else
    PaintClientEdge(canvas);
}

void OpaqueNonClientView::Layout() {
  LayoutWindowControls();
  LayoutDistributorLogo();
  LayoutTitleBar();
  LayoutOTRAvatar();
  LayoutClientView();
}

gfx::Size OpaqueNonClientView::GetPreferredSize() {
  gfx::Size prefsize(frame_->client_view()->GetPreferredSize());
  prefsize.Enlarge(2 * HorizontalBorderSize(),
      CalculateNonClientTopHeight() + VerticalBorderBottomSize());
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

int OpaqueNonClientView::CalculateNonClientTopHeight() const {
  bool show_title = frame_->window_delegate()->ShouldShowWindowTitle();
  int title_height = std::max(title_font_.height(), kWindowTitleMinimumHeight);
  if (frame_->IsMaximized()) {
    int top_height = GetSystemMetrics(SM_CYSIZEFRAME);
    if (show_title) {
      top_height += kWindowTitleZoomedTopOffset + title_height +
          kTitleZoomedBottomSpacing;
    }
    if (!browser_view_->IsToolbarVisible())
      top_height -= kClientEdgeZoomedBottomCrop;
    return top_height;
  }
  return show_title ?
      (kWindowTitleTopOffset + title_height + kTitleBottomSpacing) :
      kNoTitleTopSpacing;
}

int OpaqueNonClientView::HorizontalBorderSize() const {
  return frame_->IsMaximized() ?
      GetSystemMetrics(SM_CXSIZEFRAME) : kWindowHorizontalBorderSize;
}

int OpaqueNonClientView::VerticalBorderBottomSize() const {
  return frame_->IsMaximized() ? (GetSystemMetrics(SM_CYSIZEFRAME) +
      kWindowVerticalBorderZoomedBottomSize) : kWindowVerticalBorderBottomSize;
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
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
                       top_right_corner->height(), right_edge->width(),
                       height() - top_right_corner->height() -
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
  canvas->TileImageInt(*left_edge, 0, top_left_corner->height(),
      left_edge->width(),
      height() - top_left_corner->height() - bottom_left_corner->height());
}

void OpaqueNonClientView::PaintMaximizedFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_MAXIMIZED_TOP_EDGE);
  int frame_thickness = GetSystemMetrics(SM_CYSIZEFRAME);
  canvas->TileImageInt(*top_edge, 0, frame_thickness, width(),
                       top_edge->height());

  SkBitmap* bottom_edge =
      resources()->GetPartBitmap(FRAME_MAXIMIZED_BOTTOM_EDGE);
  canvas->TileImageInt(*bottom_edge, 0,
      height() - bottom_edge->height() - frame_thickness, width(),
      bottom_edge->height());
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
    canvas->DrawStringInt(d->GetWindowTitle(), title_font_, SK_ColorWHITE,
        MirroredLeftPointForRect(title_bounds_), title_bounds_.y(),
        title_bounds_.width(), title_bounds_.height());
    /* TODO(pkasting):
    if (app window && active)
      Draw Drop Shadow;
    */
  }
}

void OpaqueNonClientView::PaintToolbarBackground(ChromeCanvas* canvas) {
  if (!browser_view_->IsToolbarVisible() && !browser_view_->IsTabStripVisible())
    return;

  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->client_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  SkBitmap* toolbar_left =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
  canvas->DrawBitmapInt(*toolbar_left,
                        toolbar_bounds.x() - toolbar_left->width(),
                        toolbar_bounds.y());

  SkBitmap* toolbar_center =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
  canvas->TileImageInt(*toolbar_center, toolbar_bounds.x(), toolbar_bounds.y(),
                       toolbar_bounds.width(), toolbar_center->height());

  canvas->DrawBitmapInt(
      *resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT),
      toolbar_bounds.right(), toolbar_bounds.y());
}

void OpaqueNonClientView::PaintOTRAvatar(ChromeCanvas* canvas) {
  if (browser_view_->ShouldShowOffTheRecordAvatar()) {
    int src_y = frame_->IsMaximized() ? kOTRZoomedTopCrop : 0;
    canvas->DrawBitmapInt(browser_view_->GetOTRAvatarIcon(), 0, src_y,
        otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), 
        MirroredLeftPointForRect(otr_avatar_bounds_), otr_avatar_bounds_.y(),
        otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), false);
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
  // Note: We can get away with this hackery because we only draw a top edge
  // when there is no toolbar.  If we tried to draw a top edge over the
  // toolbar's top edge, we'd need a different solution.
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  if (browser_view_->IsToolbarVisible())
    toolbar_bounds.set_height(std::max(0, toolbar_bounds.height() - 1));
  int client_area_top = frame_->client_view()->y() + toolbar_bounds.bottom();

  // When we don't have a toolbar to draw a top edge for us, draw a top edge.
  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());
  if (!browser_view_->IsToolbarVisible()) {
    // This is necessary because the top center bitmap is shorter than the top
    // left and right bitmaps.  We need their top edges to line up, and we
    // need the left and right edges to start below the corners' bottoms.
    int top_edge_y = client_area_top - app_top_center_.height();
    client_area_top = top_edge_y + app_top_left_.height();
    canvas->DrawBitmapInt(app_top_left_,
                          client_area_bounds.x() - app_top_left_.width(),
                          top_edge_y);
    canvas->TileImageInt(app_top_center_, client_area_bounds.x(), top_edge_y,
                         client_area_bounds.width(), app_top_center_.height());
    canvas->DrawBitmapInt(app_top_right_, client_area_bounds.right(),
                          top_edge_y);
  }

  int client_area_bottom =
      std::max(client_area_top, height() - VerticalBorderBottomSize());
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

void OpaqueNonClientView::PaintMaximizedClientEdge(ChromeCanvas* canvas) {
  if (browser_view_->IsToolbarVisible())
    return;  // The toolbar draws its own client edge, which is sufficient.

  int edge_height = app_top_center_.height() - kClientEdgeZoomedBottomCrop;
  canvas->TileImageInt(app_top_center_, 0,
      frame_->client_view()->y() - edge_height, width(), edge_height);
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
  int top_extra_height = is_maximized ?
      (GetSystemMetrics(SM_CYSIZEFRAME) + kWindowControlsZoomedTopExtraHeight) :
      0;
  int right_offset = is_maximized ?
      (GetSystemMetrics(SM_CXSIZEFRAME) + kWindowControlsZoomedRightOffset) :
      kWindowControlsRightOffset;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      (width() - close_button_size.width() - right_offset),
      top_offset,
      (is_maximized ?
          // We extend the maximized close button to the screen corner to obey
          // Fitts' Law.
          (close_button_size.width() + kWindowControlsZoomedRightOffset) :
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

void OpaqueNonClientView::LayoutDistributorLogo() {
  logo_bounds_.SetRect(minimize_button_->x() - distributor_logo_.width() -
      kDistributorLogoHorizontalOffset, kDistributorLogoVerticalOffset,
      distributor_logo_.width(), distributor_logo_.height());
}

void OpaqueNonClientView::LayoutTitleBar() {
  // Size the window icon, even if it is hidden so we can size the title based
  // on its position.
  int left_offset = frame_->IsMaximized() ?
      (GetSystemMetrics(SM_CXSIZEFRAME) + kWindowIconZoomedLeftOffset) :
      kWindowIconLeftOffset;

  // The usable height of the titlebar area is the total height minus the top
  // resize border, the one shadow pixel at the bottom, and any client edge area
  // we draw below that shadow pixel (only in restored mode).
  // TODO(pkasting): Clean up this "4" hack.
  int top_border_height =
      frame_->IsMaximized() ? GetSystemMetrics(SM_CYSIZEFRAME) : 4;
  int available_height = CalculateNonClientTopHeight() - top_border_height - 1;
  if (!frame_->IsMaximized())
    available_height -= kClientEdgeZoomedBottomCrop;

  // The icon takes up a constant fraction of the available height, down to a
  // minimum size, and is always an even number of pixels on a side (presumably
  // to make scaled icons look better).  It's centered within the usable height.
  int icon_size = std::max((available_height * kWindowIconFractionNumerator /
      kWindowIconFractionDenominator) / 2 * 2, kWindowIconMinimumSize);
  int icon_top_offset =
      ((available_height - icon_size) / 2) + top_border_height;

  // Hack: Our frame border has a different "3D look" than Windows'.  Theirs has
  // a more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered in
  // the remaining space.  Because the apparent shape of our border is simpler,
  // using the same positioning makes things look slightly uncentered with
  // restored windows, so we come up one pixel to compensate.
  if (!frame_->IsMaximized())
    --icon_top_offset;

  views::WindowDelegate* d = frame_->window_delegate();
  if (!d->ShouldShowWindowIcon())
    icon_size = 0;
  icon_bounds_.SetRect(left_offset, icon_top_offset, icon_size, icon_size);
  if (window_icon_)
    window_icon_->SetBounds(icon_bounds_);

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    int title_right = logo_bounds_.x() - kTitleLogoSpacing;
    int icon_right = icon_bounds_.right();
    int title_left =
        icon_right + (d->ShouldShowWindowIcon() ? kWindowIconTitleSpacing : 0);
    int title_top_offset = frame_->IsMaximized() ?
        (GetSystemMetrics(SM_CYSIZEFRAME) + kWindowTitleZoomedTopOffset) :
        kWindowTitleTopOffset;
    if (title_font_.height() < kWindowTitleMinimumHeight) {
      title_top_offset +=
          (kWindowTitleMinimumHeight - title_font_.height()) / 2;
    }
    title_bounds_.SetRect(title_left, title_top_offset,
        std::max(0, title_right - icon_right), title_font_.height());
  }
}

void OpaqueNonClientView::LayoutOTRAvatar() {
  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  int non_client_height = CalculateNonClientTopHeight();
  int otr_bottom = non_client_height + browser_view_->GetTabStripHeight() -
      kOTRBottomSpacing;
  int otr_x, otr_y, otr_height;
  if (frame_->IsMaximized()) {
    otr_x = GetSystemMetrics(SM_CXSIZEFRAME) + kOTRZoomedLeftOffset;
    otr_y = non_client_height + kOTRZoomedTopSpacing;
    otr_height = otr_bottom - otr_y;
  } else {
    otr_x = kOTRLeftOffset; 
    otr_height = otr_avatar_icon.height();
    otr_y = otr_bottom - otr_height;
  }
  otr_avatar_bounds_.SetRect(otr_x, otr_y, otr_avatar_icon.width(), otr_height);
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
