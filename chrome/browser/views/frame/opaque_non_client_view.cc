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

#include "chrome/browser/views/frame/opaque_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/client_view.h"

///////////////////////////////////////////////////////////////////////////////
// WindowResources
//
// An enumeration of bitmap resources used by this window.
enum FramePartBitmap {
  FRAME_PART_BITMAP_FIRST = 0, // must be first.

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

class WindowResources {
 public:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const = 0;
  virtual const ChromeFont& GetTitleFont() const = 0;
  SkColor title_color() const { return SK_ColorWHITE; }
};

class ActiveWindowResources : public WindowResources {
 public:
  ActiveWindowResources() { InitClass(); }
  virtual ~ActiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ActiveWindowResources);
};

class InactiveWindowResources : public WindowResources {
 public:
  InactiveWindowResources() { InitClass(); }
  virtual ~InactiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(InactiveWindowResources);
};

class OTRActiveWindowResources : public WindowResources {
 public:
  OTRActiveWindowResources() { InitClass(); }
  virtual ~OTRActiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(OTRActiveWindowResources);
};

class OTRInactiveWindowResources : public WindowResources {
 public:
  OTRInactiveWindowResources() { InitClass(); }
  virtual ~OTRInactiveWindowResources() { }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(OTRInactiveWindowResources);
};
// static
SkBitmap* ActiveWindowResources::standard_frame_bitmaps_[];
ChromeFont ActiveWindowResources::title_font_;
SkBitmap* InactiveWindowResources::standard_frame_bitmaps_[];
ChromeFont InactiveWindowResources::title_font_;
SkBitmap* OTRActiveWindowResources::standard_frame_bitmaps_[];
ChromeFont OTRActiveWindowResources::title_font_;
SkBitmap* OTRInactiveWindowResources::standard_frame_bitmaps_[];
ChromeFont OTRInactiveWindowResources::title_font_;

WindowResources* OpaqueNonClientView::active_resources_ = NULL;
WindowResources* OpaqueNonClientView::inactive_resources_ = NULL;
WindowResources* OpaqueNonClientView::active_otr_resources_ = NULL;
WindowResources* OpaqueNonClientView::inactive_otr_resources_ = NULL;
SkBitmap OpaqueNonClientView::distributor_logo_;
static const int kWindowControlsTopOffset = 0;
static const int kWindowControlsRightOffset = 4;
static const int kWindowControlsTopZoomedOffset = 6;
static const int kWindowControlsRightZoomedOffset = 5;
static const int kWindowTopMarginZoomed = 1;
static const int kWindowIconLeftOffset = 5;
static const int kWindowIconTopOffset = 5;
static const int kTitleTopOffset = 6;
static const int kWindowIconTitleSpacing = 3;
static const int kTitleBottomSpacing = 6;
static const int kNoTitleTopSpacing = 10;
static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowHorizontalBorderSize = 4;
static const int kWindowVerticalBorderSize = 4;
static const int kWindowIconSize = 16;
static const int kDistributorLogoHorizontalOffset = 7;
static const int kDistributorLogoVerticalOffset = 3;

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, public:

OpaqueNonClientView::OpaqueNonClientView(OpaqueFrame* frame, bool is_otr)
    : minimize_button_(new ChromeViews::Button),
      maximize_button_(new ChromeViews::Button),
      restore_button_(new ChromeViews::Button),
      close_button_(new ChromeViews::Button),
      frame_(frame) {
  InitClass();
  if (is_otr) {
    current_active_resources_ = active_otr_resources_;
    current_inactive_resources_= inactive_otr_resources_;
  } else {
    current_active_resources_ = active_resources_;
    current_inactive_resources_ = inactive_resources_;
  }

  WindowResources* resources = current_active_resources_;
  minimize_button_->SetImage(
      ChromeViews::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON));
  minimize_button_->SetImage(
      ChromeViews::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_H));
  minimize_button_->SetImage(
      ChromeViews::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_P));
  minimize_button_->SetListener(this, -1);
  AddChildView(minimize_button_);

  maximize_button_->SetImage(
      ChromeViews::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON));
  maximize_button_->SetImage(
      ChromeViews::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_H));
  maximize_button_->SetImage(
      ChromeViews::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_P));
  maximize_button_->SetListener(this, -1);
  AddChildView(maximize_button_);

  restore_button_->SetImage(
      ChromeViews::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON));
  restore_button_->SetImage(
      ChromeViews::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_H));
  restore_button_->SetImage(
      ChromeViews::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_P));
  restore_button_->SetListener(this, -1);
  AddChildView(restore_button_);

  close_button_->SetImage(
      ChromeViews::Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(
      ChromeViews::Button::BS_HOT,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(
      ChromeViews::Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetListener(this, -1);
  AddChildView(close_button_);
}

OpaqueNonClientView::~OpaqueNonClientView() {
}

gfx::Rect OpaqueNonClientView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) {
  int top_height = CalculateNonClientTopHeight();
  // TODO(beng): support popups.
  //top_height += browser_view_->GetToolbarHeightForPopup();
  int window_x = std::max(0, client_bounds.x() - kWindowHorizontalBorderSize);
  int window_y = std::max(0, client_bounds.y() - top_height);
  int window_w = client_bounds.width() + (2 * kWindowHorizontalBorderSize);
  int window_h =
      client_bounds.height() + top_height + kWindowVerticalBorderSize;
  return gfx::Rect(window_x, window_y, window_w, window_h);
}

gfx::Rect OpaqueNonClientView::GetBoundsForTabStrip(TabStrip* tabstrip) {
  int tabstrip_height = tabstrip->GetPreferredHeight();
  int tabstrip_x = frame_->client_view()->GetX() - 4;
  return gfx::Rect(tabstrip_x, frame_->client_view()->GetY() - 1,
                   minimize_button_->GetX() - tabstrip_x, tabstrip_height);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, ChromeViews::BaseButton::ButtonListener implementation:

void OpaqueNonClientView::ButtonPressed(ChromeViews::BaseButton* sender) {
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
// OpaqueNonClientView, ChromeViews::NonClientView implementation:

gfx::Rect OpaqueNonClientView::CalculateClientAreaBounds(int width,
                                                         int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Rect(kWindowHorizontalBorderSize, top_margin,
      std::max(0, width - (2 * kWindowHorizontalBorderSize)),
      std::max(0, height - top_margin - kWindowVerticalBorderSize));
}

gfx::Size OpaqueNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Size(width + (2 * kWindowHorizontalBorderSize),
                   height + top_margin + kWindowVerticalBorderSize);
}

CPoint OpaqueNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(icon_bounds_.x(), icon_bounds_.bottom());
  MapWindowPoints(frame_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int OpaqueNonClientView::NonClientHitTest(const gfx::Point& point) {
  CRect bounds;
  CPoint test_point = point.ToPOINT();

  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = frame_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  // Then see if the point is within any of the window controls.
  close_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTCLOSE;
  restore_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTMAXBUTTON;
  maximize_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTMAXBUTTON;
  minimize_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTMINBUTTON;
  /*
  system_menu_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTSYSMENU;
  */

  component = GetHTComponentForFrame(
      point,
      kResizeAreaSize,
      kResizeAreaCornerSize,
      kResizeAreaNorthSize,
      frame_->window_delegate()->CanResize());
  if (component == HTNOWHERE) {
    // Finally fall back to the caption.
    GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
    if (bounds.PtInRect(test_point))
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

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, ChromeViews::View overrides:

void OpaqueNonClientView::Paint(ChromeCanvas* canvas) {
  if (frame_->IsMaximized()) {
    PaintMaximizedFrameBorder(canvas);
  } else {
    PaintFrameBorder(canvas);
  }
  PaintDistributorLogo(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  PaintClientEdge(canvas);

  // TODO(beng): remove this
  gfx::Rect contents_bounds = frame_->GetContentsBounds();
  canvas->FillRectInt(SK_ColorGRAY, contents_bounds.x(), contents_bounds.y(),
                      contents_bounds.width(), contents_bounds.height());
}

void OpaqueNonClientView::Layout() {
  LayoutWindowControls();
  LayoutDistributorLogo();
  LayoutTitleBar();
  LayoutClientView();
}

void OpaqueNonClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  frame_->client_view()->GetPreferredSize(out);
  out->cx += 2 * kWindowHorizontalBorderSize;
  out->cy += CalculateNonClientTopHeight() + kWindowVerticalBorderSize;
}

ChromeViews::View* OpaqueNonClientView::GetViewForPoint(
    const CPoint& point,
    bool can_create_floating) {
  // We override this function because the ClientView can overlap the non -
  // client view, making it impossible to click on the window controls. We need
  // to ensure the window controls are checked _first_.
  ChromeViews::View* views[] = { close_button_, restore_button_,
                                 maximize_button_, minimize_button_ };
  for (int i = 0; i < arraysize(views); ++i) {
    if (!views[i]->IsVisible())
      continue;
    CRect bounds;
    views[i]->GetBounds(&bounds);
    if (bounds.PtInRect(point))
      return views[i];
  }
  return View::GetViewForPoint(point, can_create_floating);
}

void OpaqueNonClientView::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

void OpaqueNonClientView::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  if (is_add && child == this) {
    DCHECK(GetViewContainer());
    DCHECK(frame_->client_view()->GetParent() != this);
    AddChildView(frame_->client_view());
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueNonClientView, private:

void OpaqueNonClientView::SetWindowIcon(SkBitmap window_icon) {

}

int OpaqueNonClientView::CalculateNonClientTopHeight() const {
  if (frame_->window_delegate()->ShouldShowWindowTitle()) {
    return kTitleTopOffset + resources()->GetTitleFont().height() +
        kTitleBottomSpacing;
  }
  return kNoTitleTopSpacing;
}

void OpaqueNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
  int width = GetWidth();
  int height = GetHeight();

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
                       width - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width - top_right_corner->width(), 0);

  // Right.
  int top_stack_height = top_right_corner->height();
  canvas->TileImageInt(*right_edge, width - right_edge->width(),
                       top_stack_height, right_edge->width(),
                       height - top_stack_height -
                           bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width - bottom_right_corner->width(),
                        height - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
                       height - bottom_edge->height(),
                       width - bottom_left_corner->width() -
                           bottom_right_corner->width(),
                       bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height - bottom_left_corner->height());

  // Left.
  top_stack_height = top_left_corner->height();
  canvas->TileImageInt(*left_edge, 0, top_stack_height, left_edge->width(),
                       height - top_stack_height -
                           bottom_left_corner->height());
}

void OpaqueNonClientView::PaintMaximizedFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_MAXIMIZED_TOP_EDGE);
  SkBitmap* bottom_edge =
      resources()->GetPartBitmap(FRAME_MAXIMIZED_BOTTOM_EDGE);
  canvas->TileImageInt(*top_edge, 0, 0, GetWidth(), top_edge->height());
  canvas->TileImageInt(*bottom_edge, 0, GetHeight() - bottom_edge->height(),
                       GetWidth(), bottom_edge->height());
}

void OpaqueNonClientView::PaintDistributorLogo(ChromeCanvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized.
  if (!frame_->IsMaximized() && !frame_->IsMinimized()) {
    canvas->DrawBitmapInt(distributor_logo_, logo_bounds_.x(),
                          logo_bounds_.y());
  }
}

void OpaqueNonClientView::PaintTitleBar(ChromeCanvas* canvas) {
  ChromeViews::WindowDelegate* d = frame_->window_delegate();
  if (d->ShouldShowWindowIcon()) {
    canvas->DrawBitmapInt(d->GetWindowIcon(), icon_bounds_.x(),
                          icon_bounds_.y());
  }
  if (d->ShouldShowWindowTitle()) {
    canvas->DrawStringInt(d->GetWindowTitle(),
                          resources()->GetTitleFont(),
                          resources()->title_color(), title_bounds_.x(),
                          title_bounds_.y(), title_bounds_.width(),
                          title_bounds_.height());
  }
}

void OpaqueNonClientView::PaintToolbarBackground(ChromeCanvas* canvas) {
  if (frame_->IsToolbarVisible() || frame_->IsTabStripVisible()) {
    SkBitmap* toolbar_left =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
    SkBitmap* toolbar_center =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
    SkBitmap* toolbar_right =
        resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);

    gfx::Rect toolbar_bounds = frame_->GetToolbarBounds();
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
  SkBitmap* right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  SkBitmap* bottom_right =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT);
  SkBitmap* bottom = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  SkBitmap* bottom_left =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  SkBitmap* left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);

  // The toolbar renders its own client edge in PaintToolbarBackground, however
  // there are other bands that need to have a client edge rendered along their
  // sides, such as the Bookmark bar, infobars, etc.
  gfx::Rect toolbar_bounds = frame_->GetToolbarBounds();
  gfx::Rect client_area_bounds = frame_->GetContentsBounds();
  client_area_bounds.SetRect(
      client_area_bounds.x(),
      frame_->client_view()->GetY() + toolbar_bounds.bottom() - 1,
      client_area_bounds.width(),
      std::max(0, GetHeight() - frame_->client_view()->GetY() -
          toolbar_bounds.bottom() + 1 - kWindowVerticalBorderSize));

  canvas->TileImageInt(*right, client_area_bounds.right(),
                       client_area_bounds.y(),
                       right->width(), client_area_bounds.height());
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
                       client_area_bounds.bottom(),
                       client_area_bounds.width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
                        client_area_bounds.x() - bottom_left->width(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
                       client_area_bounds.y(),
                       left->width(), client_area_bounds.height());
}

void OpaqueNonClientView::LayoutWindowControls() {
  CSize ps;
  if (frame_->IsMaximized() || frame_->IsMinimized()) {
    maximize_button_->SetVisible(false);
    restore_button_->SetVisible(true);
  }

  if (frame_->IsMaximized()) {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                     ChromeViews::Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        GetWidth() - ps.cx - kWindowControlsRightZoomedOffset,
        0, ps.cx + kWindowControlsRightZoomedOffset,
        ps.cy + kWindowControlsTopZoomedOffset);

    restore_button_->GetPreferredSize(&ps);
    restore_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                       ChromeViews::Button::ALIGN_BOTTOM);
    restore_button_->SetBounds(close_button_->GetX() - ps.cx, 0, ps.cx,
                               ps.cy + kWindowControlsTopZoomedOffset);

    minimize_button_->GetPreferredSize(&ps);
    minimize_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                        ChromeViews::Button::ALIGN_BOTTOM);
    minimize_button_->SetBounds(restore_button_->GetX() - ps.cx, 0, ps.cx,
                                ps.cy + kWindowControlsTopZoomedOffset);
  } else if (frame_->IsMinimized()) {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                     ChromeViews::Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        GetWidth() - ps.cx - kWindowControlsRightZoomedOffset,
        0, ps.cx + kWindowControlsRightZoomedOffset,
        ps.cy + kWindowControlsTopZoomedOffset);

    restore_button_->GetPreferredSize(&ps);
    restore_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                       ChromeViews::Button::ALIGN_BOTTOM);
    restore_button_->SetBounds(close_button_->GetX() - ps.cx, 0, ps.cx,
                               ps.cy + kWindowControlsTopZoomedOffset);

    minimize_button_->GetPreferredSize(&ps);
    minimize_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                        ChromeViews::Button::ALIGN_BOTTOM);
    minimize_button_->SetBounds(restore_button_->GetX() - ps.cx, 0, ps.cx,
                                ps.cy + kWindowControlsTopZoomedOffset);
  } else {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                     ChromeViews::Button::ALIGN_TOP);
    close_button_->SetBounds(GetWidth() - kWindowControlsRightOffset - ps.cx,
                             kWindowControlsTopOffset, ps.cx, ps.cy);

    restore_button_->SetVisible(false);

    maximize_button_->SetVisible(true);
    maximize_button_->GetPreferredSize(&ps);
    maximize_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                        ChromeViews::Button::ALIGN_TOP);
    maximize_button_->SetBounds(close_button_->GetX() - ps.cx,
                                kWindowControlsTopOffset, ps.cx, ps.cy);

    minimize_button_->GetPreferredSize(&ps);
    minimize_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                        ChromeViews::Button::ALIGN_TOP);
    minimize_button_->SetBounds(maximize_button_->GetX() - ps.cx,
                                kWindowControlsTopOffset, ps.cx, ps.cy);
  }
}

void OpaqueNonClientView::LayoutDistributorLogo() {
  int logo_w = distributor_logo_.width();
  int logo_h = distributor_logo_.height();
  
  logo_bounds_.SetRect(
      minimize_button_->GetX() - logo_w - kDistributorLogoHorizontalOffset,
      kDistributorLogoVerticalOffset, logo_w, logo_h);
}

void OpaqueNonClientView::LayoutTitleBar() {
  int top_offset = frame_->IsMaximized() ? kWindowTopMarginZoomed : 0;
  ChromeViews::WindowDelegate* d = frame_->window_delegate();

  // Size the window icon, if visible.
  if (d->ShouldShowWindowIcon()) {
    icon_bounds_.SetRect(kWindowIconLeftOffset, kWindowIconLeftOffset,
                         kWindowIconSize, kWindowIconSize);
  } else {
    // Put the menu in the right place at least even if it is hidden so we
    // can size the title based on its position.
    icon_bounds_.SetRect(kWindowIconLeftOffset, kWindowIconTopOffset, 0, 0);
  }

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    int spacing = d->ShouldShowWindowIcon() ? kWindowIconTitleSpacing : 0;
    int title_right = minimize_button_->GetX();
    int title_left = icon_bounds_.right() + spacing;
    title_bounds_.SetRect(title_left, kTitleTopOffset + top_offset,
        std::max(0, static_cast<int>(title_right - icon_bounds_.right())),
        resources()->GetTitleFont().height());
  }
}

void OpaqueNonClientView::LayoutClientView() {
  gfx::Rect client_bounds(
      CalculateClientAreaBounds(GetWidth(), GetHeight()));
  frame_->client_view()->SetBounds(client_bounds.ToRECT());
}

// static
void OpaqueNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    active_resources_ = new ActiveWindowResources;
    inactive_resources_ = new InactiveWindowResources;
    active_otr_resources_ = new OTRActiveWindowResources;
    inactive_otr_resources_ = new OTRInactiveWindowResources;

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    distributor_logo_ = *rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO_LIGHT);

    initialized = true;
  }
}
