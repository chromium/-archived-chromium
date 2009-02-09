// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/custom_frame_window.h"

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "base/win_util.h"
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
#include "chrome/views/root_view.h"
#include "chrome/views/window_delegate.h"
#include "chrome/views/window_resources.h"
#include "generated_resources.h"

namespace views {

// A scoping class that prevents a window from being able to redraw in response
// to invalidations that may occur within it for the lifetime of the object.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Occasionally, Windows will paint portions of the default non-client area
// right over the top of the custom frame. This is not simply fixed by handling
// WM_NCPAINT/WM_PAINT, with some investigation it turns out that this
// rendering is being done *inside* the default implementation of some message
// handlers and functions:
//  . WM_SETTEXT
//  . WM_SETICON
//  . WM_NCLBUTTONDOWN
//  . EnableMenuItem, called from our WM_INITMENU handler
// The solution is to handle these messages and call DefWindowProc ourselves,
// but prevent the window from being able to update itself for the duration of
// the call. We do this with this class, which automatically calls its
// associated CustomFrameWindow's lock and unlock functions as it is created
// and destroyed. See documentation in those methods for the technique used.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class CustomFrameWindow::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(CustomFrameWindow* window) : window_(window) {
    window_->LockUpdates();
  }

  ~ScopedRedrawLock() {
    window_->UnlockUpdates();
  }

 private:
  // The window having its style changed.
  CustomFrameWindow* window_;
};

HCURSOR CustomFrameWindow::resize_cursors_[6];

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
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(InactiveWindowResources);
};

// static
SkBitmap* ActiveWindowResources::standard_frame_bitmaps_[];
SkBitmap* InactiveWindowResources::standard_frame_bitmaps_[];

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
  virtual void ResetWindowControls();

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // BaseButton::ButtonListener implementation:
  virtual void ButtonPressed(BaseButton* sender);

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // A bottom border, and, in restored mode, a client edge are drawn at the
  // bottom of the titlebar.  This returns the total height drawn.
  int BottomEdgeThicknessWithinNonClientHeight() const;

  // Calculates multiple values related to title layout.  Returns the height of
  // the entire titlebar including any connected client edge.
  int TitleCoordinates(int* title_top_spacing,
                       int* title_thickness) const;

  // Paint various sub-components of this view.
  void PaintRestoredFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintRestoredClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the resource collection to be used when rendering the window.
  WindowResources* resources() const {
    return container_->is_active() || paint_as_active() ? active_resources_
                                                        : inactive_resources_;
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

  // The window that owns this view.
  CustomFrameWindow* container_;

  // Initialize various static resources.
  static void InitClass();
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultNonClientView);
};

// static
WindowResources* DefaultNonClientView::active_resources_ = NULL;
WindowResources* DefaultNonClientView::inactive_resources_ = NULL;
ChromeFont DefaultNonClientView::title_font_;

namespace {
// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// Various edges of the frame border have a 1 px shadow along their edges; in a
// few cases we shift elements based on this amount for visual appeal.
const int kFrameShadowThickness = 1;
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
// There is a 5 px gap between the title text and the caption buttons.
const int kTitleCaptionSpacing = 5;
// The caption buttons are always drawn 1 px down from the visible top of the
// window (the true top in restored mode, or the top of the screen in maximized
// mode).
const int kCaptionTopSpacing = 1;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, public:

DefaultNonClientView::DefaultNonClientView(
    CustomFrameWindow* container)
    : NonClientView(),
      client_view_(NULL),
      close_button_(new Button),
      restore_button_(new Button),
      maximize_button_(new Button),
      minimize_button_(new Button),
      system_menu_button_(new Button),
      should_show_minmax_buttons_(false),
      container_(container) {
  InitClass();
  WindowResources* resources = active_resources_;

  // Close button images will be set in LayoutWindowControls().
  close_button_->SetListener(this, -1);
  AddChildView(close_button_);

  restore_button_->SetImage(Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON));
  restore_button_->SetImage(Button::BS_HOT,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_H));
  restore_button_->SetImage(Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_P));
  restore_button_->SetListener(this, -1);
  AddChildView(restore_button_);

  maximize_button_->SetImage(Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON));
  maximize_button_->SetImage(Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_H));
  maximize_button_->SetImage(Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_P));
  maximize_button_->SetListener(this, -1);
  AddChildView(maximize_button_);

  minimize_button_->SetImage(Button::BS_NORMAL,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON));
  minimize_button_->SetImage(Button::BS_HOT,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_H));
  minimize_button_->SetImage(Button::BS_PUSHED,
      resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_P));
  minimize_button_->SetListener(this, -1);
  AddChildView(minimize_button_);

  should_show_minmax_buttons_ = container->window_delegate()->CanMaximize();

  AddChildView(system_menu_button_);
}

DefaultNonClientView::~DefaultNonClientView() {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, CustomFrameWindow::NonClientView implementation:

gfx::Rect DefaultNonClientView::CalculateClientAreaBounds(int width,
                                                          int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

gfx::Size DefaultNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int border_thickness = NonClientBorderThickness();
  return gfx::Size(width + (2 * border_thickness),
                   height + NonClientTopBorderHeight() + border_thickness);
}

CPoint DefaultNonClientView::GetSystemMenuPoint() const {
  // TODO(pkasting): This is wrong; Windows native runs the menu at the bottom
  // of the titlebar, not the bottom of the window icon.
  CPoint system_menu_point(system_menu_button_->x(),
      system_menu_button_->y() + system_menu_button_->height());
  MapWindowPoints(container_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int DefaultNonClientView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = container_->client_view()->NonClientHitTest(point);
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
  if (system_menu_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTSYSMENU;

  int window_component = GetHTComponentForFrame(point, FrameBorderThickness(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      container_->window_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void DefaultNonClientView::GetWindowMask(const gfx::Size& size,
                                         gfx::Path* window_mask) {
  DCHECK(window_mask);

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

void DefaultNonClientView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void DefaultNonClientView::ResetWindowControls() {
  restore_button_->SetState(Button::BS_NORMAL);
  minimize_button_->SetState(Button::BS_NORMAL);
  maximize_button_->SetState(Button::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, View overrides:

void DefaultNonClientView::Paint(ChromeCanvas* canvas) {
  if (container_->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);
  PaintTitleBar(canvas);
  if (!container_->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void DefaultNonClientView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
}

void DefaultNonClientView::ViewHierarchyChanged(bool is_add,
                                                View* parent,
                                                View* child) {
  // Add our Client View as we are added to the Widget so that if we are
  // subsequently resized all the parent-child relationships are established.
  if (is_add && GetWidget() && child == this)
    AddChildView(container_->client_view());
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, BaseButton::ButtonListener implementation:

void DefaultNonClientView::ButtonPressed(BaseButton* sender) {
  if (sender == close_button_)
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
  else if (sender == minimize_button_)
    container_->ExecuteSystemMenuCommand(SC_MINIMIZE);
  else if (sender == maximize_button_)
    container_->ExecuteSystemMenuCommand(SC_MAXIMIZE);
  else if (sender == restore_button_)
    container_->ExecuteSystemMenuCommand(SC_RESTORE);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, private:

int DefaultNonClientView::FrameBorderThickness() const {
  return container_->IsMaximized() ?
      GetSystemMetrics(SM_CXSIZEFRAME) : kFrameBorderThickness;
}

int DefaultNonClientView::NonClientBorderThickness() const {
  // In maximized mode, we don't show a client edge.
  return FrameBorderThickness() +
      (container_->IsMaximized() ? 0 : kClientEdgeThickness);
}

int DefaultNonClientView::NonClientTopBorderHeight() const {
  int title_top_spacing, title_thickness;
  return TitleCoordinates(&title_top_spacing, &title_thickness);
}

int DefaultNonClientView::BottomEdgeThicknessWithinNonClientHeight() const {
  return kFrameShadowThickness +
      (container_->IsMaximized() ? 0 : kClientEdgeThickness);
}

int DefaultNonClientView::TitleCoordinates(int* title_top_spacing,
                                          int* title_thickness) const {
  int frame_thickness = FrameBorderThickness();
  int min_titlebar_height = kTitlebarMinimumHeight + frame_thickness;
  *title_top_spacing = frame_thickness + kTitleTopSpacing;
  // The bottom spacing should be the same apparent height as the top spacing.
  // Because the actual top spacing height varies based on the system border
  // thickness, we calculate this based on the restored top spacing and then
  // adjust for maximized mode.  We also don't include the frame shadow here,
  // since while it's part of the bottom spacing it will be added in at the end.
  int title_bottom_spacing =
      kFrameBorderThickness + kTitleTopSpacing - kFrameShadowThickness;
  if (container_->IsMaximized()) {
    // When we maximize, the top border appears to be chopped off; shift the
    // title down to stay centered within the remaining space.
    int title_adjust = (kFrameBorderThickness / 2);
    *title_top_spacing += title_adjust;
    title_bottom_spacing -= title_adjust;
  }
  *title_thickness = std::max(title_font_.height(),
      min_titlebar_height - *title_top_spacing - title_bottom_spacing);
  return *title_top_spacing + *title_thickness + title_bottom_spacing +
      BottomEdgeThicknessWithinNonClientHeight();
}

void DefaultNonClientView::PaintRestoredFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_left_corner = resources()->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
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

void DefaultNonClientView::PaintMaximizedFrameBorder(
    ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_TOP_EDGE);
  canvas->TileImageInt(*top_edge, 0, FrameBorderThickness(), width(),
                       top_edge->height());

  // The bottom of the titlebar actually comes from the top of the Client Edge
  // graphic, with the actual client edge clipped off the bottom.
  SkBitmap* titlebar_bottom = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
  int edge_height = titlebar_bottom->height() - kClientEdgeThickness;
  canvas->TileImageInt(*titlebar_bottom, 0,
      container_->client_view()->y() - edge_height, width(), edge_height);
}

void DefaultNonClientView::PaintTitleBar(ChromeCanvas* canvas) {
  WindowDelegate* d = container_->window_delegate();

  // It seems like in some conditions we can be asked to paint after the window
  // that contains us is WM_DESTROYed. At this point, our delegate is NULL. The
  // correct long term fix may be to shut down the RootView in WM_DESTROY.
  if (!d)
    return;

  canvas->DrawStringInt(d->GetWindowTitle(), title_font_, SK_ColorWHITE,
      MirroredLeftPointForRect(title_bounds_), title_bounds_.y(),
      title_bounds_.width(), title_bounds_.height());
}

void DefaultNonClientView::PaintRestoredClientEdge(ChromeCanvas* canvas) {
  gfx::Rect client_area_bounds = container_->client_view()->bounds();
  int client_area_top = client_area_bounds.y();

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

  // Top.
  // This next calculation is necessary because the top center bitmap is shorter
  // than the top left and right bitmaps.  We need their top edges to line up,
  // and we need the left and right edges to start below the corners' bottoms.
  int top_edge_y = client_area_top - top->height();
  client_area_top = top_edge_y + top_left->height();
  canvas->DrawBitmapInt(*top_left, client_area_bounds.x() - top_left->width(),
                        top_edge_y);
  canvas->TileImageInt(*top, client_area_bounds.x(), top_edge_y,
                       client_area_bounds.width(), top->height());
  canvas->DrawBitmapInt(*top_right, client_area_bounds.right(), top_edge_y);

  // Right.
  int client_area_bottom =
      std::max(client_area_top, client_area_bounds.bottom());
  int client_area_height = client_area_bottom - client_area_top;
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bottom);
  canvas->TileImageInt(*bottom, client_area_bounds.x(), client_area_bottom,
                       client_area_bounds.width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  // Left.
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);
}

void DefaultNonClientView::LayoutWindowControls() {
  close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  bool is_maximized = container_->IsMaximized();
  int frame_thickness = FrameBorderThickness();
  int caption_y = is_maximized ? frame_thickness : kCaptionTopSpacing;
  int top_extra_height = is_maximized ? kCaptionTopSpacing : 0;
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the rightmost
  // button to the screen corner to obey Fitts' Law.
  int right_extra_width = is_maximized ?
      (kFrameBorderThickness - kFrameShadowThickness) : 0;
  int right_spacing = is_maximized ?
      (GetSystemMetrics(SM_CXSIZEFRAME) + right_extra_width) : frame_thickness;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - close_button_size.width() - right_spacing,
                           caption_y,
                           close_button_size.width() + right_extra_width,
                           close_button_size.height() + top_extra_height);

  // When the window is restored, we show a maximized button; otherwise, we show
  // a restore button.
  bool is_restored = !is_maximized && !container_->IsMinimized();
  views::Button* invisible_button = is_restored ?
      restore_button_ : maximize_button_;
  invisible_button->SetVisible(false);

  views::Button* visible_button = is_restored ?
      maximize_button_ : restore_button_;
  FramePartBitmap normal_part, hot_part, pushed_part;
  if (should_show_minmax_buttons_) {
    visible_button->SetVisible(true);
    visible_button->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
    gfx::Size visible_button_size = visible_button->GetPreferredSize();
    visible_button->SetBounds(close_button_->x() - visible_button_size.width(),
                              caption_y, visible_button_size.width(),
                              visible_button_size.height() + top_extra_height);

    minimize_button_->SetVisible(true);
    minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                        Button::ALIGN_BOTTOM);
    gfx::Size minimize_button_size = minimize_button_->GetPreferredSize();
    minimize_button_->SetBounds(
        visible_button->x() - minimize_button_size.width(), caption_y,
        minimize_button_size.width(),
        minimize_button_size.height() + top_extra_height);

    normal_part = FRAME_CLOSE_BUTTON_ICON;
    hot_part = FRAME_CLOSE_BUTTON_ICON_H;
    pushed_part = FRAME_CLOSE_BUTTON_ICON_P;
  } else {
    visible_button->SetVisible(false);
    minimize_button_->SetVisible(false);

    normal_part = FRAME_CLOSE_BUTTON_ICON_SA;
    hot_part = FRAME_CLOSE_BUTTON_ICON_SA_H;
    pushed_part = FRAME_CLOSE_BUTTON_ICON_SA_P;
  }

  close_button_->SetImage(Button::BS_NORMAL,
                          active_resources_->GetPartBitmap(normal_part));
  close_button_->SetImage(Button::BS_HOT,
                          active_resources_->GetPartBitmap(hot_part));
  close_button_->SetImage(Button::BS_PUSHED,
                          active_resources_->GetPartBitmap(pushed_part));
}

void DefaultNonClientView::LayoutTitleBar() {
  // Always lay out the icon, even when it's not present, so we can lay out the
  // window title based on its position.
  int frame_thickness = FrameBorderThickness();
  int icon_x = frame_thickness + kIconLeftSpacing;

  // The usable height of the titlebar area is the total height minus the top
  // resize border and any edge area we draw at its bottom.
  int title_top_spacing, title_thickness;
  int top_height = TitleCoordinates(&title_top_spacing, &title_thickness);
  int available_height = top_height - frame_thickness -
      BottomEdgeThicknessWithinNonClientHeight();

  // The icon takes up a constant fraction of the available height, down to a
  // minimum size, and is always an even number of pixels on a side (presumably
  // to make scaled icons look better).  It's centered within the usable height.
  int icon_size = std::max((available_height * kIconHeightFractionNumerator /
      kIconHeightFractionDenominator) / 2 * 2, kIconMinimumSize);
  int icon_y = ((available_height - icon_size) / 2) + frame_thickness;

  // Hack: Our frame border has a different "3D look" than Windows'.  Theirs has
  // a more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered in
  // the remaining space.  Because the apparent shape of our border is simpler,
  // using the same positioning makes things look slightly uncentered with
  // restored windows, so we come up to compensate.
  if (!container_->IsMaximized())
    icon_y -= kIconRestoredAdjust;

  views::WindowDelegate* d = container_->window_delegate();
  if (!d->ShouldShowWindowIcon())
    icon_size = 0;
  system_menu_button_->SetBounds(icon_x, icon_y, icon_size, icon_size);

  // Size the title.
  int icon_right = icon_x + icon_size;
  int title_x =
      icon_right + (d->ShouldShowWindowIcon() ? kIconTitleSpacing : 0);
  int title_right = (should_show_minmax_buttons_ ?
      minimize_button_->x() : close_button_->x()) - kTitleCaptionSpacing;
  title_bounds_.SetRect(title_x,
      title_top_spacing + ((title_thickness - title_font_.height()) / 2),
      std::max(0, title_right - title_x), title_font_.height());
}

void DefaultNonClientView::LayoutClientView() {
  container_->client_view()->SetBounds(CalculateClientAreaBounds(width(),
                                                                 height()));
}

// static
void DefaultNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    active_resources_ = new ActiveWindowResources;
    inactive_resources_ = new InactiveWindowResources;

    title_font_ = win_util::GetWindowTitleFont();

    initialized = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, public:

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate)
    : Window(window_delegate),
      is_active_(false),
      lock_updates_(false),
      saved_window_style_(0) {
  InitClass();
  non_client_view_ = new DefaultNonClientView(this);
}

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate,
                                     NonClientView* non_client_view)
    : Window(window_delegate) {
  InitClass();
  non_client_view_ = non_client_view;
}

CustomFrameWindow::~CustomFrameWindow() {
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
  WidgetWin::SetContentsView(non_client_view_);
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
  // Must call the base class too so that places like the Task Bar get updated.
  Window::UpdateWindowTitle();
}

void CustomFrameWindow::UpdateWindowIcon() {
  // The icon will be re-validated during painting.
  non_client_view_->SchedulePaint();
  // Call the base class so that places like the Task Bar get updated.
  Window::UpdateWindowIcon();
}

void CustomFrameWindow::EnableClose(bool enable) {
  non_client_view_->EnableClose(enable);
  // Make sure the SysMenu changes to reflect this change as well.
  Window::EnableClose(enable);
}

void CustomFrameWindow::DisableInactiveRendering(bool disable) {
  Window::DisableInactiveRendering(disable);
  non_client_view_->set_paint_as_active(disable);
  if (!disable)
    non_client_view_->SchedulePaint();
}

void CustomFrameWindow::SizeWindowToDefault() {
  gfx::Size pref = client_view()->GetPreferredSize();
  DCHECK(pref.width() > 0 && pref.height() > 0);
  gfx::Size window_size =
      non_client_view_->CalculateWindowSizeForClientSize(pref.width(),
                                                         pref.height());
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(),
                                window_size.ToSIZE(), false);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, WidgetWin overrides:

static void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

void CustomFrameWindow::OnInitMenu(HMENU menu) {
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItem(menu, SC_RESTORE, !is_restored);
  EnableMenuItem(menu, SC_MOVE, is_restored);
  EnableMenuItem(menu, SC_SIZE, window_delegate()->CanResize() && is_restored);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 window_delegate()->CanMaximize() && !is_maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 window_delegate()->CanMaximize() && !is_minimized);
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
  if (IsWindowVisible(GetHWND())) {
    non_client_view_->SchedulePaint();
    // We need to force a paint now, as a user dragging a window will block
    // painting operations while the move is in progress.
    PaintNow(root_view_->GetScheduledPaintRect());
  }

  return TRUE;
}

LRESULT CustomFrameWindow::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We need to repaint all when the window bounds change.
  return WVR_REDRAW;
}

LRESULT CustomFrameWindow::OnNCHitTest(const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  return non_client_view_->NonClientHitTest(gfx::Point(temp.x, temp.y));
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

  if (window_rect.Width() != root_view_->width() ||
      window_rect.Height() != root_view_->height()) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }

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

  root_view->SchedulePaint(gfx::Rect(dirty_region), false);

  // ChromeCanvasPaints destructor does the actual painting. As such, wrap the
  // following in a block to force paint to occur so that we can release the dc.
  {
    ChromeCanvasPaint canvas(dc, opaque(), dirty_region.left, dirty_region.top,
                             dirty_region.Width(), dirty_region.Height());

    root_view->ProcessPaint(&canvas);
  }

  ReleaseDC(GetHWND(), dc);
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
      ProcessNCMousePress(point, MK_LBUTTON);
      return;
    }
    default:
      Window::OnNCLButtonDown(ht_component, point);
      /*
      if (!IsMsgHandled()) {
        // Window::OnNCLButtonDown set the message as unhandled. This normally
        // means WidgetWin::ProcessWindowMessage will pass it to
        // DefWindowProc. Sadly, DefWindowProc for WM_NCLBUTTONDOWN does weird
        // non-client painting, so we need to call it directly here inside a
        // scoped update lock.
        ScopedRedrawLock lock(this);
        DefWindowProc(GetHWND(), WM_NCLBUTTONDOWN, ht_component,
                      MAKELPARAM(point.x, point.y));
        SetMsgHandled(TRUE);
      }
      */
      break;
  }
}

void CustomFrameWindow::OnNCMButtonDown(UINT ht_component,
                                        const CPoint& point) {
  if (ht_component == HTCAPTION) {
    // When there's only one window and only one tab, the tab area is reported
    // to be part of the caption area of the window. However users should still
    // be able to middle click that tab to close it so we need to make sure
    // these messages reach the View system.
    ProcessNCMousePress(point, MK_MBUTTON);
    SetMsgHandled(FALSE);
    return;
  }
  WidgetWin::OnNCMButtonDown(ht_component, point);
}

LRESULT CustomFrameWindow::OnNCUAHDrawCaption(UINT msg, WPARAM w_param,
                                              LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(TRUE);
  return 0;
}

LRESULT CustomFrameWindow::OnNCUAHDrawFrame(UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(TRUE);
  return 0;
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

LRESULT CustomFrameWindow::OnSetIcon(UINT size_type, HICON new_icon) {
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETICON, size_type,
                       reinterpret_cast<LPARAM>(new_icon));
}

LRESULT CustomFrameWindow::OnSetText(const wchar_t* text) {
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETTEXT, NULL,
                       reinterpret_cast<LPARAM>(text));
}

void CustomFrameWindow::OnSize(UINT param, const CSize& size) {
  Window::OnSize(param, size);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been layed out.
  ResetWindowRegion();
}

void CustomFrameWindow::OnSysCommand(UINT notification_code, CPoint click) {
  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  if ((notification_code & sc_mask) == SC_MINIMIZE ||
      (notification_code & sc_mask) == SC_MAXIMIZE ||
      (notification_code & sc_mask) == SC_RESTORE) {
    non_client_view_->ResetWindowControls();
  } else if ((notification_code & sc_mask) == SC_MOVE ||
             (notification_code & sc_mask) == SC_SIZE) {
    if (lock_updates_) {
      // We were locked, before entering a resize or move modal loop. Now that
      // we've begun to move the window, we need to unlock updates so that the
      // sizing/moving feedback can be continuous.
      UnlockUpdates();
    }
  }
  Window::OnSysCommand(notification_code, click);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, private:

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

void CustomFrameWindow::LockUpdates() {
  lock_updates_ = true;
  saved_window_style_ = GetWindowLong(GetHWND(), GWL_STYLE);
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_ & ~WS_VISIBLE);
}

void CustomFrameWindow::UnlockUpdates() {
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_);
  lock_updates_ = false;
}

void CustomFrameWindow::ResetWindowRegion() {
  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HRGN current_rgn = CreateRectRgn(0, 0, 0, 0);
  int current_rgn_result = GetWindowRgn(GetHWND(), current_rgn);

  CRect window_rect;
  GetWindowRect(&window_rect);
  HRGN new_region;
  if (IsMaximized()) {
    HMONITOR monitor = MonitorFromWindow(GetHWND(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    GetMonitorInfo(monitor, &mi);
    CRect work_rect = mi.rcWork;
    work_rect.OffsetRect(-window_rect.left, -window_rect.top);
    new_region = CreateRectRgnIndirect(&work_rect);
  } else {
    gfx::Path window_mask;
    non_client_view_->GetWindowMask(gfx::Size(window_rect.Width(),
                                              window_rect.Height()),
                                    &window_mask);
    new_region = window_mask.CreateHRGN();
  }

  if (current_rgn_result == ERROR || !EqualRgn(current_rgn, new_region)) {
    // SetWindowRgn takes ownership of the HRGN created by CreateHRGN.
    SetWindowRgn(new_region, TRUE);
  } else {
    DeleteObject(new_region);
  }

  DeleteObject(current_rgn);
}

void CustomFrameWindow::ProcessNCMousePress(const CPoint& point, int flags) {
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  UINT message_flags = 0;
  if ((GetKeyState(VK_CONTROL) & 0x80) == 0x80)
    message_flags |= MK_CONTROL;
  if ((GetKeyState(VK_SHIFT) & 0x80) == 0x80)
    message_flags |= MK_SHIFT;
  message_flags |= flags;
  ProcessMousePressed(temp, message_flags, false);
}

}  // namespace views

