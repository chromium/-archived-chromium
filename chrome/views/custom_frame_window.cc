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

#include "chrome/views/custom_frame_window.h"

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/client_view.h"
#include "chrome/views/native_button.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/window_delegate.h"
#include "generated_resources.h"

namespace ChromeViews {

HCURSOR CustomFrameWindow::resize_cursors_[6];

///////////////////////////////////////////////////////////////////////////////
// WindowResources
//
// An enumeration of bitmap resources used by this window.
enum FramePartBitmap {
  FRAME_PART_BITMAP_FIRST = 0,  // must be first.

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
  ActiveWindowResources() {
    InitClass();
  }
  virtual ~ActiveWindowResources() {
  }

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
        IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER, IDR_APP_TOP_RIGHT,
            IDR_CONTENT_RIGHT_SIDE, IDR_CONTENT_BOTTOM_RIGHT_CORNER,
            IDR_CONTENT_BOTTOM_CENTER, IDR_CONTENT_BOTTOM_LEFT_CORNER,
            IDR_CONTENT_LEFT_SIDE,
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
  InactiveWindowResources() {
    InitClass();
  }
  virtual ~InactiveWindowResources() {
  }

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
        IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER, IDR_APP_TOP_RIGHT,
            IDR_CONTENT_RIGHT_SIDE, IDR_CONTENT_BOTTOM_RIGHT_CORNER,
            IDR_CONTENT_BOTTOM_CENTER, IDR_CONTENT_BOTTOM_LEFT_CORNER,
            IDR_CONTENT_LEFT_SIDE,
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

// static
SkBitmap* ActiveWindowResources::standard_frame_bitmaps_[];
ChromeFont ActiveWindowResources::title_font_;
SkBitmap* InactiveWindowResources::standard_frame_bitmaps_[];
ChromeFont InactiveWindowResources::title_font_;


///////////////////////////////////////////////////////////////////////////////
//
// DefaultNonClientView
//
//  A ChromeView that provides the "frame" for CustomFrameWindows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class DefaultNonClientView : public NonClientView,
                             public BaseButton::ButtonListener {
 public:
  explicit DefaultNonClientView(CustomFrameWindow* container);
  virtual ~DefaultNonClientView();

  // Overridden from CustomFrameWindow::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // BaseButton::ButtonListener implementation:
  virtual void ButtonPressed(BaseButton* sender);

 private:
  // Updates the system menu icon button.
  void SetWindowIcon(SkBitmap window_icon);

  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateContentsTop() const;

  // Paint various sub-components of this view.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the resource collection to be used when rendering the window.
  WindowResources* resources() const {
    return container_->is_active() ? active_resources_ : inactive_resources_;
  }

  // The View that provides the background for the window, and optionally
  // dialog buttons. Note: the non-client view does _not_ own this view, the
  // container does.
  ClientView* client_view_;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Window controls.
  Button* close_button_;
  Button* restore_button_;
  Button* maximize_button_;
  Button* minimize_button_;
  Button* system_menu_button_;  // Uses the window icon if visible.
  bool should_show_minmax_buttons_;

  // The window icon.
  SkBitmap window_icon_;

  // The window that owns this view.
  CustomFrameWindow* container_;

  // Initialize various static resources.
  static void InitClass();
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultNonClientView);
};

// static
WindowResources* DefaultNonClientView::active_resources_ = NULL;
WindowResources* DefaultNonClientView::inactive_resources_ = NULL;
static const int kWindowControlsTopOffset = 1;
static const int kWindowControlsRightOffset = 5;
static const int kWindowControlsTopZoomedOffset = 1;
static const int kWindowControlsRightZoomedOffset = 5;
static const int kWindowTopMarginZoomed = 1;
static const int kWindowIconLeftOffset = 5;
static const int kWindowIconTopOffset = 5;
static const int kTitleTopOffset = 6;
static const int kWindowIconTitleSpacing = 3;
static const int kTitleBottomSpacing = 6;
static const int kNoTitleTopSpacing = 8;
static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowHorizontalBorderSize = 4;
static const int kWindowVerticalBorderSize = 4;

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, public:

DefaultNonClientView::DefaultNonClientView(
    CustomFrameWindow* container)
    : client_view_(NULL),
      close_button_(new Button),
      restore_button_(new Button),
      maximize_button_(new Button),
      minimize_button_(new Button),
      system_menu_button_(new Button),
      should_show_minmax_buttons_(false),
      container_(container) {
  InitClass();
  WindowResources* resources = active_resources_;

  close_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetListener(this, -1);
  AddChildView(close_button_);

  restore_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON));
  restore_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_H));
  restore_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_P));
  restore_button_->SetListener(this, -1);
  AddChildView(restore_button_);

  maximize_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON));
  maximize_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_H));
  maximize_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_P));
  maximize_button_->SetListener(this, -1);
  AddChildView(maximize_button_);

  minimize_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON));
  minimize_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_H));
  minimize_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_P));
  minimize_button_->SetListener(this, -1);
  AddChildView(minimize_button_);

  AddChildView(system_menu_button_);
}

DefaultNonClientView::~DefaultNonClientView() {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, CustomFrameWindow::NonClientView implementation:

gfx::Rect DefaultNonClientView::CalculateClientAreaBounds(
    int width, int height) const {
  int top_margin = CalculateContentsTop();
  return gfx::Rect(kWindowHorizontalBorderSize, top_margin,
      std::max(0, width - (2 * kWindowHorizontalBorderSize)),
      std::max(0, height - top_margin - kWindowVerticalBorderSize));
}

gfx::Size DefaultNonClientView::CalculateWindowSizeForClientSize(
    int width, int height) const {
  int contents_top = CalculateContentsTop();
  return gfx::Size(
      width + (2 * kWindowHorizontalBorderSize),
      height + kWindowVerticalBorderSize + contents_top);
}

CPoint DefaultNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(
      system_menu_button_->GetX(),
      system_menu_button_->GetY() + system_menu_button_->GetHeight());
  MapWindowPoints(container_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

// There is a subtle point that needs to be explained regarding the manner in
// which this function returns the HT* code Windows is expecting:
//
// |point| contains the cursor position in this View's coordinate system. If
// this View uses a right-to-left UI layout, the position represented by
// |point| will not reflect the UI mirroring because we don't create the
// container's HWND with WS_EX_LAYOUTRTL. Therefore, whenever the cursor
// position resides within the boundaries of one of our child Views (for
// example, the close_button_), we must retrieve the child View bounds such
// that bound are mirrored if the View uses right-to-left UI layout. This is
// why this function passes APPLY_MIRRORING_TRANSFORMATION as the |settings|
// whenever it calls GetBounds().
int DefaultNonClientView::NonClientHitTest(const gfx::Point& point) {
  CRect bounds;
  CPoint test_point = point.ToPOINT();

  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = container_->client_view()->NonClientHitTest(point);
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
  system_menu_button_->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (bounds.PtInRect(test_point))
    return HTSYSMENU;

  component = GetHTComponentForFrame(
      point,
      kResizeAreaSize,
      kResizeAreaCornerSize,
      kResizeAreaNorthSize,
      container_->window_delegate()->CanResize());
  if (component == HTNOWHERE) {
    // Finally fall back to the caption.
    GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
    if (bounds.PtInRect(test_point))
      component = HTCAPTION;
    // Otherwise, the point is outside the window's bounds.
  }
  return component;
}

void DefaultNonClientView::GetWindowMask(const gfx::Size& size,
                                         gfx::Path* window_mask) {
  DCHECK(window_mask);

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 3);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void DefaultNonClientView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, View overrides:

void DefaultNonClientView::Paint(ChromeCanvas* canvas) {
  if (container_->IsMaximized()) {
    PaintMaximizedFrameBorder(canvas);
  } else {
    PaintFrameBorder(canvas);
  }
  PaintClientEdge(canvas);

  WindowDelegate* d = container_->window_delegate();
  if (d->ShouldShowWindowTitle()) {
    canvas->DrawStringInt(d->GetWindowTitle(),
                          resources()->GetTitleFont(),
                          resources()->title_color(), title_bounds_.x(),
                          title_bounds_.y(), title_bounds_.width(),
                          title_bounds_.height());
  }
}

void DefaultNonClientView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
  SchedulePaint();
}

void DefaultNonClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  container_->client_view()->GetPreferredSize(out);
  out->cx += 2 * kWindowHorizontalBorderSize;
  out->cy += CalculateContentsTop() + kWindowVerticalBorderSize;
}

void DefaultNonClientView::ViewHierarchyChanged(bool is_add,
                                                View* parent,
                                                View* child) {
  // Add our Client View as we are added to the ViewContainer so that if we are
  // subsequently resized all the parent-child relationships are established.
  if (is_add && GetViewContainer() && child == this)
    AddChildView(container_->client_view());
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, BaseButton::ButtonListener implementation:

void DefaultNonClientView::ButtonPressed(BaseButton* sender) {
  if (sender == close_button_) {
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
  } else if (sender == minimize_button_) {
    container_->ExecuteSystemMenuCommand(SC_MINIMIZE);
  } else if (sender == maximize_button_) {
    container_->ExecuteSystemMenuCommand(SC_MAXIMIZE);
  } else if (sender == restore_button_) {
    container_->ExecuteSystemMenuCommand(SC_RESTORE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, private:

void DefaultNonClientView::SetWindowIcon(SkBitmap window_icon) {
  // TODO(beng): (Cleanup) remove this persistent cache of the icon when Button
  //             takes a SkBitmap rather than SkBitmap*.
  window_icon_ = window_icon;
  system_menu_button_->SetImage(Button::BS_NORMAL, &window_icon);
}

int DefaultNonClientView::CalculateContentsTop() const {
  if (container_->window_delegate()->ShouldShowWindowTitle()) {
    return kTitleTopOffset + resources()->GetTitleFont().height() +
        kTitleBottomSpacing;
  }
  return kNoTitleTopSpacing;
}

void DefaultNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
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

void DefaultNonClientView::PaintMaximizedFrameBorder(
    ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* bottom_edge =
      resources()->GetPartBitmap(FRAME_BOTTOM_EDGE);
  canvas->TileImageInt(*top_edge, 0, 0, GetWidth(), top_edge->height());
  canvas->TileImageInt(*bottom_edge, 0, GetHeight() - bottom_edge->height(),
                       GetWidth(), bottom_edge->height());
}

void DefaultNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  SkBitmap* top_left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
  SkBitmap* top = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
  SkBitmap* top_right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);
  SkBitmap* right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  SkBitmap* bottom_right =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT);
  SkBitmap* bottom = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  SkBitmap* bottom_left =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  SkBitmap* left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);

  CRect client_area_bounds;
  container_->client_view()->GetBounds(&client_area_bounds);

  canvas->DrawBitmapInt(*top_left, client_area_bounds.left - top_left->width(),
                        client_area_bounds.top - top->height());
  canvas->TileImageInt(*top, client_area_bounds.left,
                       client_area_bounds.top - top->height(),
                       client_area_bounds.Width(), top->height());
  canvas->DrawBitmapInt(*top_right, client_area_bounds.right,
                        client_area_bounds.top - top->height());
  canvas->TileImageInt(*right, client_area_bounds.right,
                       client_area_bounds.top - top->height() +
                           top_right->height(),
                       right->width(), client_area_bounds.Height());
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right,
                        client_area_bounds.bottom);
  canvas->TileImageInt(*bottom, client_area_bounds.left,
                       client_area_bounds.bottom,
                       client_area_bounds.Width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
                        client_area_bounds.left - bottom_left->width(),
                        client_area_bounds.bottom);
  canvas->TileImageInt(*left, client_area_bounds.left - left->width(),
                       client_area_bounds.top - top->height() +
                           top_left->height(),
                       left->width(), client_area_bounds.Height());
}

void DefaultNonClientView::LayoutWindowControls() {
  CSize ps;
  if (container_->IsMaximized() || container_->IsMinimized()) {
    maximize_button_->SetVisible(false);
    restore_button_->SetVisible(true);
  }

  if (container_->IsMaximized()) {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        GetWidth() - ps.cx - kWindowControlsRightZoomedOffset,
        0, ps.cx + kWindowControlsRightZoomedOffset,
        ps.cy + kWindowControlsTopZoomedOffset);

    if (should_show_minmax_buttons_) {
      restore_button_->GetPreferredSize(&ps);
      restore_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                         Button::ALIGN_BOTTOM);
      restore_button_->SetBounds(close_button_->GetX() - ps.cx, 0, ps.cx,
                                 ps.cy + kWindowControlsTopZoomedOffset);

      minimize_button_->GetPreferredSize(&ps);
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_BOTTOM);
      minimize_button_->SetBounds(restore_button_->GetX() - ps.cx, 0, ps.cx,
                                  ps.cy + kWindowControlsTopZoomedOffset);
    }
  } else if (container_->IsMinimized()) {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        GetWidth() - ps.cx - kWindowControlsRightZoomedOffset,
        0, ps.cx + kWindowControlsRightZoomedOffset,
        ps.cy + kWindowControlsTopZoomedOffset);

    if (should_show_minmax_buttons_) {
      restore_button_->GetPreferredSize(&ps);
      restore_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                         Button::ALIGN_BOTTOM);
      restore_button_->SetBounds(close_button_->GetX() - ps.cx, 0, ps.cx,
                                 ps.cy + kWindowControlsTopZoomedOffset);

      minimize_button_->GetPreferredSize(&ps);
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_BOTTOM);
      minimize_button_->SetBounds(restore_button_->GetX() - ps.cx, 0, ps.cx,
                                  ps.cy + kWindowControlsTopZoomedOffset);
    }
  } else {
    close_button_->GetPreferredSize(&ps);
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_TOP);
    close_button_->SetBounds(GetWidth() - kWindowControlsRightOffset - ps.cx,
                             kWindowControlsTopOffset, ps.cx, ps.cy);

    if (should_show_minmax_buttons_) {
      close_button_->SetImage(
          Button::BS_NORMAL,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
      close_button_->SetImage(
          Button::BS_HOT,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
      close_button_->SetImage(
          Button::BS_PUSHED,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));

      restore_button_->SetVisible(false);

      maximize_button_->SetVisible(true);
      maximize_button_->GetPreferredSize(&ps);
      maximize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_TOP);
      maximize_button_->SetBounds(close_button_->GetX() - ps.cx,
                                  kWindowControlsTopOffset, ps.cx, ps.cy);

      minimize_button_->GetPreferredSize(&ps);
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_TOP);
      minimize_button_->SetBounds(maximize_button_->GetX() - ps.cx,
                                  kWindowControlsTopOffset, ps.cx, ps.cy);
    }
  }
  if (!should_show_minmax_buttons_) {
    close_button_->SetImage(
        Button::BS_NORMAL,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA));
    close_button_->SetImage(
        Button::BS_HOT,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA_H));
    close_button_->SetImage(
        Button::BS_PUSHED,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA_P));

    restore_button_->SetVisible(false);
    maximize_button_->SetVisible(false);
    minimize_button_->SetVisible(false);
  }
}

void DefaultNonClientView::LayoutTitleBar() {
  int top_offset = container_->IsMaximized() ? kWindowTopMarginZoomed : 0;
  WindowDelegate* d = container_->window_delegate();

  // Size the window icon, if visible.
  if (d->ShouldShowWindowIcon()) {
    system_menu_button_->SetVisible(true);
    CSize ps;
    system_menu_button_->GetPreferredSize(&ps);
    system_menu_button_->SetBounds(
        kWindowIconLeftOffset, kWindowIconTopOffset + top_offset, ps.cx, ps.cy);
  } else {
    // Put the menu in the right place at least even if it is hidden so we
    // can size the title based on its position.
    system_menu_button_->SetBounds(kWindowIconLeftOffset,
                                   kWindowIconTopOffset, 0, 0);
  }

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    CRect system_menu_bounds;
    system_menu_button_->GetBounds(&system_menu_bounds);
    int spacing = d->ShouldShowWindowIcon() ? kWindowIconTitleSpacing : 0;
    int title_right = should_show_minmax_buttons_ ?
        minimize_button_->GetX() : close_button_->GetX();
    int title_left = system_menu_bounds.right + spacing;
    title_bounds_.SetRect(title_left, kTitleTopOffset + top_offset,
        std::max(0, static_cast<int>(title_right - system_menu_bounds.right)),
        resources()->GetTitleFont().height());

    // We draw the custom frame window's title directly rather than using a
    // ChromeViews::Label child view. Therefore, we have to mirror the title
    // position manually if the View's UI layout is right-to-left. Child Views
    // are automatically mirrored, which means that the parent view doesn't
    // need to manually modify their position depending on the View's UI
    // layout.
    //
    // Mirroring the title's position manually is certainly far from being
    // elegant, but we have no choice (other than changing the
    // DefaultNonClientView subclass to use a ChromeView::Label as a child View
    // instead of drawing the title's text directly on the canvas).
    title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));
  }
}

void DefaultNonClientView::LayoutClientView() {
  gfx::Rect client_bounds(
      CalculateClientAreaBounds(GetWidth(), GetHeight()));
  container_->client_view()->SetBounds(client_bounds.ToRECT());
}

// static
void DefaultNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    active_resources_ = new ActiveWindowResources;
    inactive_resources_ = new InactiveWindowResources;
    initialized = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, public:

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate)
    : Window(window_delegate),
      non_client_view_(new DefaultNonClientView(this)),
      is_active_(false) {
  InitClass();
}

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate,
                                     NonClientView* non_client_view)
    : Window(window_delegate),
      non_client_view_(non_client_view) {
  InitClass();
}

CustomFrameWindow::~CustomFrameWindow() {
}

void CustomFrameWindow::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(GetHWND(), WM_SYSCOMMAND, command, 0);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, Window overrides:

void CustomFrameWindow::Init(HWND parent, const gfx::Rect& bounds) {
  // TODO(beng): (Cleanup) Right now, the only way to specify a different
  //             non-client view is to subclass this object and provide one
  //             by setting this member before calling Init.
  if (!non_client_view_)
    non_client_view_ = new DefaultNonClientView(this);
  Window::Init(parent, bounds);
  ResetWindowRegion();
}

void CustomFrameWindow::SetClientView(ClientView* cv) {
  DCHECK(cv && !client_view() && GetHWND());
  set_client_view(cv);
  // For a CustomFrameWindow, the non-client view is the root.
  HWNDViewContainer::SetContentsView(non_client_view_);
  // When the non client view is added to the view hierarchy, it will cause the
  // client view to be added as well.
}

gfx::Size CustomFrameWindow::CalculateWindowSizeForClientSize(
    const gfx::Size& client_size) const {
  return non_client_view_->CalculateWindowSizeForClientSize(
      client_size.width(), client_size.height());
}

void CustomFrameWindow::UpdateWindowTitle() {
  // Layout winds up causing the title to be re-validated during
  // string measurement.
  non_client_view_->Layout();
}

void CustomFrameWindow::EnableClose(bool enable) {
  non_client_view_->EnableClose(enable);
  // Make sure the SysMenu changes to reflect this change as well.
  Window::EnableClose(enable);
}

void CustomFrameWindow::SizeWindowToDefault() {
  CSize pref(0, 0);
  client_view()->GetPreferredSize(&pref);
  DCHECK(pref.cx > 0 && pref.cy > 0);
  gfx::Size window_size =
      non_client_view_->CalculateWindowSizeForClientSize(pref.cx, pref.cy);
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(),
                                window_size.ToSIZE(), false);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, HWNDViewContainer overrides:

static void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

void CustomFrameWindow::OnInitMenu(HMENU menu) {
  bool minimized = IsMinimized();
  bool maximized = IsMaximized();
  bool minimized_or_maximized = minimized || maximized;

  EnableMenuItem(menu, SC_RESTORE,
                 window_delegate()->CanMaximize() && minimized_or_maximized);
  EnableMenuItem(menu, SC_MOVE, !minimized_or_maximized);
  EnableMenuItem(menu, SC_SIZE,
                 window_delegate()->CanResize() && !minimized_or_maximized);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 window_delegate()->CanMaximize() && !maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 window_delegate()->CanMaximize() && !minimized);
}

void CustomFrameWindow::OnMouseLeave() {
  bool process_mouse_exited = true;
  POINT pt;
  if (GetCursorPos(&pt)) {
    LRESULT ht_component =
        ::SendMessage(GetHWND(), WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
    if (ht_component != HTNOWHERE) {
      // If the mouse moved into a part of the window's non-client area, then
      // don't send a mouse exited event since the mouse is still within the
      // bounds of the ChromeView that's rendering the frame. Note that we do
      // _NOT_ do this for windows with native frames, since in that case the
      // mouse really will have left the bounds of the RootView.
      process_mouse_exited = false;
    }
  }

  if (process_mouse_exited)
    ProcessMouseExited();
}

LRESULT CustomFrameWindow::OnNCActivate(BOOL active) {
  is_active_ = !!active;

  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (!IsMaximized() && IsWindowVisible(GetHWND())) {
    non_client_view_->SchedulePaint();
    // We need to force a paint now, as a user dragging a window will block
    // painting operations while the move is in progress.
    PaintNow(root_view_->GetScheduledPaintRect());
  }
  return TRUE;
}

LRESULT CustomFrameWindow::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  CRect client_bounds;
  if (!mode) {
    RECT* rect = reinterpret_cast<RECT*>(l_param);
    *rect = non_client_view_->CalculateClientAreaBounds(
        rect->right - rect->left, rect->bottom - rect->top).ToRECT();
  }
  return 0;
}

LRESULT CustomFrameWindow::OnNCHitTest(const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  return non_client_view_->NonClientHitTest(gfx::Point(temp.x, temp.y));
}

LRESULT CustomFrameWindow::OnNCMouseMove(UINT flags, const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  ProcessMouseMoved(temp, 0);

  // We need to process this message to stop Windows from drawing the window
  // controls as the mouse moves over the title bar area when the window is
  // maximized.
  return 0;
}

struct ClipState {
  // The window being painted.
  HWND parent;

  // DC painting to.
  HDC dc;

  // Origin of the window in terms of the screen.
  int x;
  int y;
};

// See comments in OnNCPaint for details of this function.
static BOOL CALLBACK ClipDCToChild(HWND window, LPARAM param) {
  ClipState* clip_state = reinterpret_cast<ClipState*>(param);
  if (GetParent(window) == clip_state->parent && IsWindowVisible(window)) {
    RECT bounds;
    GetWindowRect(window, &bounds);
    ExcludeClipRect(clip_state->dc,
                    bounds.left - clip_state->x,
                    bounds.top - clip_state->y,
                    bounds.right - clip_state->x,
                    bounds.bottom - clip_state->y);
  }
  return TRUE;
}

void CustomFrameWindow::OnNCPaint(HRGN rgn) {
  // We have an NC region and need to paint it. We expand the NC region to
  // include the dirty region of the root view. This is done to minimize
  // paints.
  CRect window_rect;
  GetWindowRect(&window_rect);

  CRect dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region = CRect(0, 0, window_rect.Width(), window_rect.Height());
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect))
      return;  // Dirty region doesn't intersect window bounds, bale.

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // In theory GetDCEx should do what we want, but I couldn't get it to work.
  // In particular the docs mentiond DCX_CLIPCHILDREN, but as far as I can tell
  // it doesn't work at all. So, instead we get the DC for the window then
  // manually clip out the children.
  HDC dc = GetWindowDC(GetHWND());
  ClipState clip_state;
  clip_state.x = window_rect.left;
  clip_state.y = window_rect.top;
  clip_state.parent = GetHWND();
  clip_state.dc = dc;
  EnumChildWindows(GetHWND(), &ClipDCToChild,
                   reinterpret_cast<LPARAM>(&clip_state));

  RootView* root_view = GetRootView();
  CRect old_paint_region = root_view->GetScheduledPaintRectConstrainedToSize();

  if (!old_paint_region.IsRectEmpty()) {
    // The root view has a region that needs to be painted. Include it in the
    // region we're going to paint.

    CRect tmp = dirty_region;
    UnionRect(&dirty_region, &tmp, &old_paint_region);
  }

  root_view->SchedulePaint(dirty_region, false);

  // ChromeCanvasPaints destructor does the actual painting. As such, wrap the
  // following in a block to force paint to occur so that we can release the dc.
  {
    ChromeCanvasPaint canvas(dc, opaque(), dirty_region.left, dirty_region.top,
                             dirty_region.Width(), dirty_region.Height());

    root_view->ProcessPaint(&canvas);
  }

  ReleaseDC(GetHWND(), dc);
}

void CustomFrameWindow::OnNCRButtonDown(UINT flags, const CPoint& point) {
  if (flags == HTCAPTION || flags == HTSYSMENU) {
    RunSystemMenu(point);
  } else {
    SetMsgHandled(FALSE);
  }
}

void CustomFrameWindow::OnNCLButtonDown(UINT ht_component,
                                        const CPoint& point) {
  switch (ht_component) {
    case HTCLOSE:
    case HTMINBUTTON:
    case HTMAXBUTTON: {
      // When the mouse is pressed down in these specific non-client areas, we
      // need to tell the RootView to send the mouse pressed event (which sets
      // capture, allowing subsequent WM_LBUTTONUP (note, _not_ WM_NCLBUTTONUP)
      // to fire so that the appropriate WM_SYSCOMMAND can be sent by the
      // applicable button's ButtonListener. We _have_ to do this this way
      // rather than letting Windows just send the syscommand itself (as would
      // happen if we never did this dance) because for some insane reason
      // DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed window
      // control button appearance, in the Windows classic style, over our
      // view! Ick! By handling this message we prevent Windows from doing this
      // undesirable thing, but that means we need to roll the sys-command
      // handling ourselves.
      CPoint temp = point;
      MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
      UINT flags = 0;
      if ((GetKeyState(VK_CONTROL) & 0x80) == 0x80)
        flags |= MK_CONTROL;
      if ((GetKeyState(VK_SHIFT) & 0x80) == 0x80)
        flags |= MK_SHIFT;
      flags |= MK_LBUTTON;
      ProcessMousePressed(temp, flags, false);
      SetMsgHandled(TRUE);
      return;
    }
    case HTSYSMENU:
      RunSystemMenu(non_client_view_->GetSystemMenuPoint());
      break;
  }
  SetMsgHandled(FALSE);
}

LRESULT CustomFrameWindow::OnSetCursor(HWND window, UINT hittest_code,
                                       UINT message) {
  int index = RC_NORMAL;
  switch (hittest_code) {
    case HTTOP:
    case HTBOTTOM:
      index = RC_VERTICAL;
      break;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      index = RC_NWSE;
      break;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      index = RC_NESW;
      break;
    case HTLEFT:
    case HTRIGHT:
      index = RC_HORIZONTAL;
      break;
    case HTCAPTION:
    case HTCLIENT:
      index = RC_NORMAL;
      break;
  }
  SetCursor(resize_cursors_[index]);
  return 0;
}

void CustomFrameWindow::OnSize(UINT param, const CSize& size) {
  Window::OnSize(param, size);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been layed out.
  ResetWindowRegion();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, private:

void CustomFrameWindow::RunSystemMenu(const CPoint& point) {
  HMENU system_menu = ::GetSystemMenu(GetHWND(), FALSE);
  int id = ::TrackPopupMenu(system_menu,
                            TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                            point.x, point.y, 0, GetHWND(), NULL);
  ExecuteSystemMenuCommand(id);
}

// static
void CustomFrameWindow::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resize_cursors_[RC_NORMAL] = LoadCursor(NULL, IDC_ARROW);
    resize_cursors_[RC_VERTICAL] = LoadCursor(NULL, IDC_SIZENS);
    resize_cursors_[RC_HORIZONTAL] = LoadCursor(NULL, IDC_SIZEWE);
    resize_cursors_[RC_NESW] = LoadCursor(NULL, IDC_SIZENESW);
    resize_cursors_[RC_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    initialized = true;
  }
}

void CustomFrameWindow::ResetWindowRegion() {
  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HRGN current_rgn = CreateRectRgn(0, 0, 0, 0);
  int current_rgn_result = GetWindowRgn(GetHWND(), current_rgn);

  HRGN new_region = NULL;
  if (!IsMaximized()) {
    CRect window_rect;
    GetWindowRect(&window_rect);
    gfx::Path window_mask;
    non_client_view_->GetWindowMask(gfx::Size(window_rect.Width(),
                                              window_rect.Height()),
                                    &window_mask);
    new_region = window_mask.CreateHRGN();
  }

  if (current_rgn_result == ERROR ||
      !EqualRgn(current_rgn, new_region)) {
    // SetWindowRgn takes ownership of the HRGN created by CreateHRGN.
    SetWindowRgn(new_region, TRUE);
  } else if (new_region) {
    DeleteObject(new_region);
  }

  DeleteObject(current_rgn);
}

}  // namespace ChromeViews
