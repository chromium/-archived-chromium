// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/constrained_window_win.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/win_util.h"
#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "views/controls/button/image_button.h"
#include "views/focus/focus_manager.h"
#include "views/window/client_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window_resources.h"

using base::TimeDelta;

namespace views {
class ClientView;
}

// An enumeration of bitmap resources used by this window.
enum {
  FRAME_PART_BITMAP_FIRST = 0,  // Must be first.

  // Window Controls.
  FRAME_CLOSE_BUTTON_ICON,
  FRAME_CLOSE_BUTTON_ICON_H,
  FRAME_CLOSE_BUTTON_ICON_P,

  // Window Frame Border.
  FRAME_BOTTOM_EDGE,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_EDGE,
  FRAME_RIGHT_EDGE,
  FRAME_TOP_EDGE,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

  FRAME_WINDOW,
  FRAME_WINDOW_INACTIVE,
  FRAME_WINDOW_INCOGNITO,
  FRAME_WINDOW_INCOGNITO_INACTIVE,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

static const int kXPFramePartIDs[] = {
    0,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
    IDR_WINDOW_BOTTOM_CENTER, IDR_WINDOW_BOTTOM_LEFT_CORNER,
    IDR_WINDOW_BOTTOM_RIGHT_CORNER, IDR_WINDOW_LEFT_SIDE,
    IDR_WINDOW_RIGHT_SIDE, IDR_WINDOW_TOP_CENTER,
    IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_RIGHT_CORNER,
    IDR_THEME_FRAME, IDR_THEME_FRAME_INACTIVE, IDR_THEME_FRAME_INCOGNITO,
    IDR_THEME_FRAME_INCOGNITO_INACTIVE,
    0 };
static const int kVistaFramePartIDs[] = {
    0,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
    IDR_CONSTRAINED_BOTTOM_CENTER_V, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER_V,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER_V, IDR_CONSTRAINED_LEFT_SIDE_V,
    IDR_CONSTRAINED_RIGHT_SIDE_V, IDR_CONSTRAINED_TOP_CENTER_V,
    IDR_CONSTRAINED_TOP_LEFT_CORNER_V, IDR_CONSTRAINED_TOP_RIGHT_CORNER_V,
    IDR_THEME_FRAME, IDR_THEME_FRAME_INACTIVE, IDR_THEME_FRAME_INCOGNITO,
    IDR_THEME_FRAME_INCOGNITO_INACTIVE,
    0 };

class XPWindowResources : public views::WindowResources {
 public:
  XPWindowResources() {
    InitClass();
  }
  virtual ~XPWindowResources() {}

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kXPFramePartIDs[i];
        if (id != 0)
          bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(XPWindowResources);
};

class VistaWindowResources : public views::WindowResources {
 public:
  VistaWindowResources() {
    InitClass();
  }
  virtual ~VistaWindowResources() {}

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kVistaFramePartIDs[i];
        if (id != 0)
          bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(VistaWindowResources);
};

SkBitmap* XPWindowResources::bitmaps_[];
SkBitmap* VistaWindowResources::bitmaps_[];

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView

class ConstrainedWindowFrameView
    : public views::NonClientFrameView,
      public views::ButtonListener {
 public:
  explicit ConstrainedWindowFrameView(ConstrainedWindowWin* container);
  virtual ~ConstrainedWindowFrameView();

  void UpdateWindowTitle();

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual bool AlwaysUseCustomFrame() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual gfx::Point GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls() { }

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ThemeChanged();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

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

  // Calculates multiple values related to title layout.  Returns the height of
  // the entire titlebar including any connected client edge.
  int TitleCoordinates(int* title_top_spacing,
                       int* title_thickness) const;

  // Paints different parts of the window to the incoming canvas.
  void PaintFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  SkColor GetTitleColor() const {
    return (container_->owner()->profile()->IsOffTheRecord() ||
        !win_util::ShouldUseVistaFrame()) ? SK_ColorWHITE : SK_ColorBLACK;
  }

  // Loads the appropriate set of WindowResources for the frame view.
  void InitWindowResources();

  ConstrainedWindowWin* container_;

  scoped_ptr<views::WindowResources> resources_;

  gfx::Rect title_bounds_;

  views::ImageButton* close_button_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  static void InitClass();

  // The font to be used to render the titlebar text.
  static gfx::Font* title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowFrameView);
};

gfx::Font* ConstrainedWindowFrameView::title_font_ = NULL;

namespace {
// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// Various edges of the frame border have a 1 px shadow along their edges; in a
// few cases we shift elements based on this amount for visual appeal.
const int kFrameShadowThickness = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks to less than 20 px tall, including the height of
// the frame border and client edge.
const int kTitlebarMinimumHeight = 20;
// The icon is inset 2 px from the left frame border.
const int kIconLeftSpacing = 2;
// The title text starts 2 px below the bottom of the top frame border.
const int kTitleTopSpacing = 2;
// There is a 5 px gap between the title text and the caption buttons.
const int kTitleCaptionSpacing = 5;
// The caption buttons are always drawn 1 px down from the visible top of the
// window (the true top in restored mode, or the top of the screen in maximized
// mode).
const int kCaptionTopSpacing = 1;

const SkColor kContentsBorderShadow = SkColorSetARGB(51, 0, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, public:

ConstrainedWindowFrameView::ConstrainedWindowFrameView(
    ConstrainedWindowWin* container)
        : NonClientFrameView(),
          container_(container),
          close_button_(new views::ImageButton(this)) {
  InitClass();
  InitWindowResources();

  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  AddChildView(close_button_);
}

ConstrainedWindowFrameView::~ConstrainedWindowFrameView() {
}

void ConstrainedWindowFrameView::UpdateWindowTitle() {
  SchedulePaint(title_bounds_, false);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::NonClientFrameView implementation:

gfx::Rect ConstrainedWindowFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool ConstrainedWindowFrameView::AlwaysUseCustomFrame() const {
  // Constrained windows always use the custom frame - they just have a
  // different set of bitmaps.
  return true;
}

gfx::Rect ConstrainedWindowFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

gfx::Point ConstrainedWindowFrameView::GetSystemMenuPoint() const {
  // Doesn't really matter, since we never show system menus on constrained
  // windows...
  gfx::Point system_menu_point(FrameBorderThickness(),
                               NonClientTopBorderHeight());
  ConvertPointToScreen(this, &system_menu_point);
  return system_menu_point;
}

int ConstrainedWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = container_->GetClientView()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point, FrameBorderThickness(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      container_->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void ConstrainedWindowFrameView::GetWindowMask(const gfx::Size& size,
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

void ConstrainedWindowFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::View implementation:

void ConstrainedWindowFrameView::Paint(gfx::Canvas* canvas) {
  PaintFrameBorder(canvas);
  PaintTitleBar(canvas);
  PaintClientEdge(canvas);
}

void ConstrainedWindowFrameView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
}

void ConstrainedWindowFrameView::ThemeChanged() {
  InitWindowResources();
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::ButtonListener implementation:

void ConstrainedWindowFrameView::ButtonPressed(views::Button* sender) {
  if (sender == close_button_)
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, private:

int ConstrainedWindowFrameView::FrameBorderThickness() const {
  return kFrameBorderThickness;
}

int ConstrainedWindowFrameView::NonClientBorderThickness() const {
  return FrameBorderThickness() + kClientEdgeThickness;
}

int ConstrainedWindowFrameView::NonClientTopBorderHeight() const {
  int title_top_spacing, title_thickness;
  return TitleCoordinates(&title_top_spacing, &title_thickness);
}

int ConstrainedWindowFrameView::TitleCoordinates(
    int* title_top_spacing,
    int* title_thickness) const {
  int frame_thickness = FrameBorderThickness();
  int min_titlebar_height = kTitlebarMinimumHeight + frame_thickness;
  *title_top_spacing = frame_thickness + kTitleTopSpacing;
  // The bottom spacing should be the same apparent height as the top spacing,
  // plus have the client edge tacked on.
  int title_bottom_spacing = *title_top_spacing + kClientEdgeThickness;
  *title_thickness = std::max(title_font_->height(),
      min_titlebar_height - *title_top_spacing - title_bottom_spacing);
  return *title_top_spacing + *title_thickness + title_bottom_spacing;
}

void ConstrainedWindowFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  SkBitmap* top_left_corner = resources_->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      resources_->GetPartBitmap(FRAME_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = resources_->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* right_edge = resources_->GetPartBitmap(FRAME_RIGHT_EDGE);
  SkBitmap* left_edge = resources_->GetPartBitmap(FRAME_LEFT_EDGE);
  SkBitmap* bottom_left_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = resources_->GetPartBitmap(FRAME_BOTTOM_EDGE);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* theme_frame = rb.GetBitmapNamed(IDR_THEME_FRAME);
  SkColor frame_color = ResourceBundle::frame_color;

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

void ConstrainedWindowFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  canvas->DrawStringInt(container_->GetWindowTitle(), *title_font_,
      GetTitleColor(), MirroredLeftPointForRect(title_bounds_),
      title_bounds_.y(), title_bounds_.width(), title_bounds_.height());
}

void ConstrainedWindowFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_edge_bounds(CalculateClientAreaBounds(width(), height()));
  client_edge_bounds.Inset(-kClientEdgeThickness, -kClientEdgeThickness);
  gfx::Rect frame_shadow_bounds(client_edge_bounds);
  frame_shadow_bounds.Inset(-kFrameShadowThickness, -kFrameShadowThickness);

  canvas->FillRectInt(kContentsBorderShadow, frame_shadow_bounds.x(),
                      frame_shadow_bounds.y(), frame_shadow_bounds.width(),
                      frame_shadow_bounds.height());

  canvas->FillRectInt(ResourceBundle::toolbar_color, client_edge_bounds.x(),
                      client_edge_bounds.y(), client_edge_bounds.width(),
                      client_edge_bounds.height());
}

void ConstrainedWindowFrameView::LayoutWindowControls() {
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - close_button_size.width() - FrameBorderThickness(),
      kCaptionTopSpacing, close_button_size.width(),
      close_button_size.height());
}

void ConstrainedWindowFrameView::LayoutTitleBar() {
  // Size the title.
  int title_x = FrameBorderThickness() + kIconLeftSpacing;
  int title_top_spacing, title_thickness;
  TitleCoordinates(&title_top_spacing, &title_thickness);
  title_bounds_.SetRect(title_x,
      title_top_spacing + ((title_thickness - title_font_->height()) / 2),
      std::max(0, close_button_->x() - kTitleCaptionSpacing - title_x),
      title_font_->height());
}

void ConstrainedWindowFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect ConstrainedWindowFrameView::CalculateClientAreaBounds(
    int width,
    int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

void ConstrainedWindowFrameView::InitWindowResources() {
  if (win_util::ShouldUseVistaFrame()) {
    resources_.reset(new VistaWindowResources);
  } else {
    resources_.reset(new XPWindowResources);
  }
}

// static
void ConstrainedWindowFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    title_font_ = new gfx::Font(win_util::GetWindowTitleFont());

    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, public:

// The space (in pixels) between minimized pop-ups stacked horizontally and
// vertically.
static const int kPopupRepositionOffset = 5;
static const int kConstrainedWindowEdgePadding = 10;

ConstrainedWindowWin::~ConstrainedWindowWin() {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, ConstrainedWindow implementation:

views::NonClientFrameView* ConstrainedWindowWin::CreateFrameViewForWindow() {
  return new ConstrainedWindowFrameView(this);
}

void ConstrainedWindowWin::CloseConstrainedWindow() {
  // Broadcast to all observers of NOTIFY_CWINDOW_CLOSED.
  // One example of such an observer is AutomationCWindowTracker in the
  // automation component.
  NotificationService::current()->Notify(NotificationType::CWINDOW_CLOSED,
                                         Source<ConstrainedWindow>(this),
                                         NotificationService::NoDetails());

  Close();
}

std::wstring ConstrainedWindowWin::GetWindowTitle() const {
  std::wstring display_title;
  if (GetDelegate())
    display_title = GetDelegate()->GetWindowTitle();
  else
    display_title = L"Untitled";

  return display_title;
}

const gfx::Rect& ConstrainedWindowWin::GetCurrentBounds() const {
  return current_bounds_;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, private:

ConstrainedWindowWin::ConstrainedWindowWin(
    TabContents* owner,
    views::WindowDelegate* window_delegate)
    : WindowWin(window_delegate),
      owner_(owner) {
  GetNonClientView()->SetFrameView(CreateFrameViewForWindow());

  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION |
                   WS_THICKFRAME | WS_SYSMENU);
  set_focus_on_creation(false);

  WindowWin::Init(owner_->GetNativeView(), gfx::Rect());
  ActivateConstrainedWindow();
}

void ConstrainedWindowWin::ActivateConstrainedWindow() {
  // Other pop-ups are simply moved to the front of the z-order.
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, views::WidgetWin overrides:

void ConstrainedWindowWin::OnDestroy() {
  // TODO(jcampan): figure out focus restoration

  // Make sure we call super so that it can do its cleanup.
  WindowWin::OnDestroy();
}

void ConstrainedWindowWin::OnFinalMessage(HWND window) {
  // Tell our constraining TabContents that we've gone so it can update its
  // list.
  owner_->WillClose(this);

  WindowWin::OnFinalMessage(window);
}

LRESULT ConstrainedWindowWin::OnMouseActivate(HWND window,
                                               UINT hittest_code,
                                               UINT message) {
  // We only detach the window if the user clicked on the title bar. That
  // way, users can click inside the contents of legitimate popups obtained
  // with a mouse gesture.
  if (hittest_code != HTCLIENT && hittest_code != HTNOWHERE &&
      hittest_code != HTCLOSE) {
    ActivateConstrainedWindow();
  }

  return MA_ACTIVATE;
}

void ConstrainedWindowWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  // If the window was moved or sized, tell the owner.
  if (!(window_pos->flags & SWP_NOMOVE) || !(window_pos->flags & SWP_NOSIZE))
    owner_->DidMoveOrResize(this);
  SetMsgHandled(FALSE);
}


// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    views::WindowDelegate* window_delegate) {
  ConstrainedWindowWin* window = new ConstrainedWindowWin(parent,
                                                          window_delegate);
  return window;
}
