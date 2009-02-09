// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/constrained_window_impl.h"

#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/client_view.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/window_resources.h"
#include "net/base/net_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

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

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

static const int kXPFramePartIDs[] = {
    0,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
    IDR_CONSTRAINED_BOTTOM_CENTER, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER, IDR_CONSTRAINED_LEFT_SIDE,
    IDR_CONSTRAINED_RIGHT_SIDE, IDR_CONSTRAINED_TOP_CENTER,
    IDR_CONSTRAINED_TOP_LEFT_CORNER, IDR_CONSTRAINED_TOP_RIGHT_CORNER,
    0 };
static const int kVistaFramePartIDs[] = {
    0,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
    IDR_CONSTRAINED_BOTTOM_CENTER_V, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER_V,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER_V, IDR_CONSTRAINED_LEFT_SIDE_V,
    IDR_CONSTRAINED_RIGHT_SIDE_V, IDR_CONSTRAINED_TOP_CENTER_V,
    IDR_CONSTRAINED_TOP_LEFT_CORNER_V, IDR_CONSTRAINED_TOP_RIGHT_CORNER_V,
    0 };
static const int kOTRFramePartIDs[] = {
    0,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
    IDR_WINDOW_BOTTOM_CENTER_OTR, IDR_WINDOW_BOTTOM_LEFT_CORNER_OTR,
    IDR_WINDOW_BOTTOM_RIGHT_CORNER_OTR, IDR_WINDOW_LEFT_SIDE_OTR,
    IDR_WINDOW_RIGHT_SIDE_OTR, IDR_WINDOW_TOP_CENTER_OTR,
    IDR_WINDOW_TOP_LEFT_CORNER_OTR, IDR_WINDOW_TOP_RIGHT_CORNER_OTR,
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

class OTRWindowResources : public views::WindowResources {
 public:
  OTRWindowResources() {
    InitClass();
  }
  virtual ~OTRWindowResources() {}

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kOTRFramePartIDs[i];
        if (id != 0)
          bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(OTRWindowResources);
};

SkBitmap* XPWindowResources::bitmaps_[];
SkBitmap* VistaWindowResources::bitmaps_[];
SkBitmap* OTRWindowResources::bitmaps_[];

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView

class ConstrainedWindowNonClientView
    : public views::NonClientView,
      public views::BaseButton::ButtonListener {
 public:
  ConstrainedWindowNonClientView(ConstrainedWindowImpl* container,
                                 TabContents* owner);
  virtual ~ConstrainedWindowNonClientView();

  void UpdateWindowTitle();

  // Overridden from views::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls() { }

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::BaseButton* sender);

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
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  SkColor GetTitleColor() const {
    return (container_->owner()->profile()->IsOffTheRecord() ||
        !win_util::ShouldUseVistaFrame()) ? SK_ColorWHITE : SK_ColorBLACK;
  }

  ConstrainedWindowImpl* container_;

  scoped_ptr<views::WindowResources> resources_;

  gfx::Rect title_bounds_;

  views::Button* close_button_;

  static void InitClass();

  // The font to be used to render the titlebar text.
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowNonClientView);
};

ChromeFont ConstrainedWindowNonClientView::title_font_;

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
const SkColor kContentsBorderColor = SkColorSetRGB(219, 235, 255);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, public:

ConstrainedWindowNonClientView::ConstrainedWindowNonClientView(
    ConstrainedWindowImpl* container, TabContents* owner)
        : NonClientView(),
          container_(container),
          close_button_(new views::Button) {
  InitClass();
  if (owner->profile()->IsOffTheRecord()) {
    resources_.reset(new OTRWindowResources);
  } else {
    if (win_util::ShouldUseVistaFrame()) {
      resources_.reset(new VistaWindowResources);
    } else {
      resources_.reset(new XPWindowResources);
    }
  }

  close_button_->SetImage(views::Button::BS_NORMAL,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(views::Button::BS_HOT,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(views::Button::BS_PUSHED,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetImageAlignment(views::Button::ALIGN_CENTER,
                                   views::Button::ALIGN_MIDDLE);
  close_button_->SetListener(this, 0);
  AddChildView(close_button_);
}

ConstrainedWindowNonClientView::~ConstrainedWindowNonClientView() {
}

void ConstrainedWindowNonClientView::UpdateWindowTitle() {
  SchedulePaint(title_bounds_, false);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, views::NonClientView implementation:

gfx::Rect ConstrainedWindowNonClientView::CalculateClientAreaBounds(
    int width,
    int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

gfx::Size ConstrainedWindowNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int border_thickness = NonClientBorderThickness();
  return gfx::Size(width + (2 * border_thickness),
                   height + NonClientTopBorderHeight() + border_thickness);
}

CPoint ConstrainedWindowNonClientView::GetSystemMenuPoint() const {
  // Doesn't matter what we return, since this is only used when the user clicks
  // a window icon, and we never have an icon.
  return CPoint();
}

int ConstrainedWindowNonClientView::NonClientHitTest(const gfx::Point& point) {
  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int frame_component = container_->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point, FrameBorderThickness(),
      NonClientBorderThickness(), kResizeAreaCornerSize,
      container_->window_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return ((window_component == HTNOWHERE) && bounds().Contains(point)) ?
      HTCAPTION : window_component;
}

void ConstrainedWindowNonClientView::GetWindowMask(const gfx::Size& size,
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

void ConstrainedWindowNonClientView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, views::View implementation:

void ConstrainedWindowNonClientView::Paint(ChromeCanvas* canvas) {
  PaintFrameBorder(canvas);
  PaintTitleBar(canvas);
  PaintClientEdge(canvas);
}

void ConstrainedWindowNonClientView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
}

gfx::Size ConstrainedWindowNonClientView::GetPreferredSize() {
  gfx::Size prefsize(container_->client_view()->GetPreferredSize());
  int border_thickness = NonClientBorderThickness();
  prefsize.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);
  return prefsize;
}

void ConstrainedWindowNonClientView::ViewHierarchyChanged(bool is_add,
                                                          View *parent,
                                                          View *child) {
  // Add our Client View as we are added to the Container so that if we are
  // subsequently resized all the parent-child relationships are established.
  if (is_add && GetWidget() && child == this)
    AddChildView(container_->client_view());
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, views::BaseButton::Button
//     implementation:

void ConstrainedWindowNonClientView::ButtonPressed(views::BaseButton* sender) {
  if (sender == close_button_)
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, private:

int ConstrainedWindowNonClientView::FrameBorderThickness() const {
  return kFrameBorderThickness;
}

int ConstrainedWindowNonClientView::NonClientBorderThickness() const {
  return FrameBorderThickness() + kClientEdgeThickness;
}

int ConstrainedWindowNonClientView::NonClientTopBorderHeight() const {
  int title_top_spacing, title_thickness;
  return TitleCoordinates(&title_top_spacing, &title_thickness);
}

int ConstrainedWindowNonClientView::TitleCoordinates(
    int* title_top_spacing,
    int* title_thickness) const {
  int frame_thickness = FrameBorderThickness();
  int min_titlebar_height = kTitlebarMinimumHeight + frame_thickness;
  *title_top_spacing = frame_thickness + kTitleTopSpacing;
  // The bottom spacing should be the same apparent height as the top spacing,
  // plus have the client edge tacked on.
  int title_bottom_spacing = *title_top_spacing + kClientEdgeThickness;
  *title_thickness = std::max(title_font_.height(),
      min_titlebar_height - *title_top_spacing - title_bottom_spacing);
  return *title_top_spacing + *title_thickness + title_bottom_spacing;
}

void ConstrainedWindowNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
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

void ConstrainedWindowNonClientView::PaintTitleBar(ChromeCanvas* canvas) {
  canvas->DrawStringInt(container_->GetWindowTitle(), title_font_,
      GetTitleColor(), MirroredLeftPointForRect(title_bounds_),
      title_bounds_.y(), title_bounds_.width(), title_bounds_.height());
}

void ConstrainedWindowNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  gfx::Rect client_edge_bounds(CalculateClientAreaBounds(width(), height()));
  client_edge_bounds.Inset(-kClientEdgeThickness, -kClientEdgeThickness);
  gfx::Rect frame_shadow_bounds(client_edge_bounds);
  frame_shadow_bounds.Inset(-kFrameShadowThickness, -kFrameShadowThickness);

  canvas->FillRectInt(kContentsBorderShadow, frame_shadow_bounds.x(),
                      frame_shadow_bounds.y(), frame_shadow_bounds.width(),
                      frame_shadow_bounds.height());

  canvas->FillRectInt(kContentsBorderColor, client_edge_bounds.x(),
                      client_edge_bounds.y(), client_edge_bounds.width(),
                      client_edge_bounds.height());
}

void ConstrainedWindowNonClientView::LayoutWindowControls() {
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - close_button_size.width() - FrameBorderThickness(),
      kCaptionTopSpacing, close_button_size.width(),
      close_button_size.height());
}

void ConstrainedWindowNonClientView::LayoutTitleBar() {
  // Size the title.
  int title_x = FrameBorderThickness() + kIconLeftSpacing;
  int title_top_spacing, title_thickness;
  TitleCoordinates(&title_top_spacing, &title_thickness);
  title_bounds_.SetRect(title_x,
      title_top_spacing + ((title_thickness - title_font_.height()) / 2),
      std::max(0, close_button_->x() - kTitleCaptionSpacing - title_x),
      title_font_.height());
}

void ConstrainedWindowNonClientView::LayoutClientView() {
  container_->client_view()->SetBounds(CalculateClientAreaBounds(width(),
                                                                 height()));
}

// static
void ConstrainedWindowNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    title_font_ = win_util::GetWindowTitleFont();

    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, public:

// The space (in pixels) between minimized pop-ups stacked horizontally and
// vertically.
static const int kPopupRepositionOffset = 5;
static const int kConstrainedWindowEdgePadding = 10;

ConstrainedWindowImpl::~ConstrainedWindowImpl() {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, ConstrainedWindow implementation:

ConstrainedWindowNonClientView* ConstrainedWindowImpl::non_client_view() {
  return static_cast<ConstrainedWindowNonClientView*>(non_client_view_);
}

void ConstrainedWindowImpl::UpdateWindowTitle() {
  UpdateUI(TabContents::INVALIDATE_TITLE);
}

void ConstrainedWindowImpl::ActivateConstrainedWindow() {
  // Other pop-ups are simply moved to the front of the z-order.
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  // Store the focus of our parent focus manager so we can restore it when we
  // close.
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetHWND());
  DCHECK(focus_manager);
  focus_manager = focus_manager->GetParentFocusManager();
  if (focus_manager) {
    // We could not have a parent focus manager if the ConstrainedWindow is
    // displayed in a tab that is not currently selected.
    // TODO(jcampan): we should store the ConstrainedWindow active events in
    // that case and replay them when the WebContents becomes selected.
    focus_manager->StoreFocusedView();

    // Give our window the focus so we get keyboard messages.
    ::SetFocus(GetHWND());
  }
}

void ConstrainedWindowImpl::CloseConstrainedWindow() {
  // Broadcast to all observers of NOTIFY_CWINDOW_CLOSED.
  // One example of such an observer is AutomationCWindowTracker in the
  // automation component.
  NotificationService::current()->Notify(NotificationType::CWINDOW_CLOSED,
                                         Source<ConstrainedWindow>(this),
                                         NotificationService::NoDetails());

  Close();
}

void ConstrainedWindowImpl::WasHidden() {
  DLOG(INFO) << "WasHidden";
}

void ConstrainedWindowImpl::DidBecomeSelected() {
  DLOG(INFO) << "DidBecomeSelected";
}

std::wstring ConstrainedWindowImpl::GetWindowTitle() const {
  std::wstring display_title;
  if (window_delegate())
    display_title = window_delegate()->GetWindowTitle();
  else
    display_title = L"Untitled";

  return display_title;
}

const gfx::Rect& ConstrainedWindowImpl::GetCurrentBounds() const {
  return current_bounds_;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, private:

ConstrainedWindowImpl::ConstrainedWindowImpl(
    TabContents* owner,
    views::WindowDelegate* window_delegate)
    : CustomFrameWindow(window_delegate,
                        new ConstrainedWindowNonClientView(this, owner)) {
  Init(owner);
}

void ConstrainedWindowImpl::Init(TabContents* owner) {
  owner_ = owner;
  focus_restoration_disabled_ = false;
  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION |
                   WS_THICKFRAME | WS_SYSMENU);
  set_focus_on_creation(false);
}

void ConstrainedWindowImpl::InitAsDialog(const gfx::Rect& initial_bounds) {
  CustomFrameWindow::Init(owner_->GetNativeView(), initial_bounds);
  ActivateConstrainedWindow();
}

void ConstrainedWindowImpl::UpdateUI(unsigned int changed_flags) {
  if (changed_flags & TabContents::INVALIDATE_TITLE)
    non_client_view()->UpdateWindowTitle();
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, views::WidgetWin overrides:

void ConstrainedWindowImpl::OnDestroy() {
  // We do this here, rather than |Close|, since the window may be destroyed in
  // a way other than by some other component calling Close, e.g. by the native
  // window hierarchy closing. We are guaranteed to receive a WM_DESTROY
  // message regardless of how the window is closed.
  // Note that when we get this message, the focus manager of the
  // ConstrainedWindow has already been destroyed (by the processing of
  // WM_DESTROY in FocusManager).  So the FocusManager we retrieve here is the
  // parent one (the one from the top window).
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetHWND());
  if (focus_manager) {
    // We may not have a focus manager if:
    // - we are hidden when closed (the TabContent would be detached).
    // - the tab has been closed and we are closed as a result.
    // TODO(jcampan): when hidden, we should modify the stored focus of the tab
    // so when it becomes visible again we retrieve the focus appropriately.
    if (!focus_restoration_disabled_)
      focus_manager->RestoreFocusedView();
  }

  // Make sure we call super so that it can do its cleanup.
  Window::OnDestroy();
}

void ConstrainedWindowImpl::OnFinalMessage(HWND window) {
  // Tell our constraining TabContents that we've gone so it can update its
  // list.
  owner_->WillClose(this);

  WidgetWin::OnFinalMessage(window);
}

LRESULT ConstrainedWindowImpl::OnMouseActivate(HWND window,
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

void ConstrainedWindowImpl::OnWindowPosChanged(WINDOWPOS* window_pos) {
  // If the window was moved or sized, tell the owner.
  if (!(window_pos->flags & SWP_NOMOVE) || !(window_pos->flags & SWP_NOSIZE))
    owner_->DidMoveOrResize(this);
  SetMsgHandled(FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow, public:

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    const gfx::Rect& initial_bounds,
    views::View* contents_view,
    views::WindowDelegate* window_delegate) {
  ConstrainedWindowImpl* window = new ConstrainedWindowImpl(parent,
                                                            window_delegate);
  window->InitAsDialog(initial_bounds);
  return window;
}
