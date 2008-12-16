// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/constrained_window_impl.h"

#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/web_app.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
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

  FRAME_BOTTOM_CENTER,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_SIDE,
  FRAME_RIGHT_SIDE,
  FRAME_TOP_CENTER,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

  FRAME_CLOSE_BUTTON_ICON,
  FRAME_CLOSE_BUTTON_ICON_H,
  FRAME_CLOSE_BUTTON_ICON_P,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

static const int kXPFramePartIDs[] = {
    0, IDR_CONSTRAINED_BOTTOM_CENTER, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER, IDR_CONSTRAINED_LEFT_SIDE,
    IDR_CONSTRAINED_RIGHT_SIDE, IDR_CONSTRAINED_TOP_CENTER,
    IDR_CONSTRAINED_TOP_LEFT_CORNER, IDR_CONSTRAINED_TOP_RIGHT_CORNER,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P, 0 };
static const int kVistaFramePartIDs[] = {
    0, IDR_CONSTRAINED_BOTTOM_CENTER_V, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER_V,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER_V, IDR_CONSTRAINED_LEFT_SIDE_V,
    IDR_CONSTRAINED_RIGHT_SIDE_V, IDR_CONSTRAINED_TOP_CENTER_V,
    IDR_CONSTRAINED_TOP_LEFT_CORNER_V, IDR_CONSTRAINED_TOP_RIGHT_CORNER_V,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P, 0 };
static const int kOTRFramePartIDs[] = {
    0, IDR_WINDOW_BOTTOM_CENTER_OTR, IDR_WINDOW_BOTTOM_LEFT_CORNER_OTR,
    IDR_WINDOW_BOTTOM_RIGHT_CORNER_OTR, IDR_WINDOW_LEFT_SIDE_OTR,
    IDR_WINDOW_RIGHT_SIDE_OTR, IDR_WINDOW_TOP_CENTER_OTR,
    IDR_WINDOW_TOP_LEFT_CORNER_OTR, IDR_WINDOW_TOP_RIGHT_CORNER_OTR,
    IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P, 0 };

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

  // Calculates the pixel height of the titlebar
  int CalculateTitlebarHeight() const;

  // Calculates the pixel height of all pieces of a window that are
  // not part of the webcontent display area.
  gfx::Rect CalculateWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  void UpdateWindowTitle();

  void set_window_delegate(views::WindowDelegate* window_delegate) {
    window_delegate_ = window_delegate;
  }

  // Overridden from views::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::BaseButton* sender);

  // Paints different parts of the window to the incoming canvas.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintWindowTitle(ChromeCanvas* canvas);

  SkColor GetTitleColor() const {
    if (container_->owner()->profile()->IsOffTheRecord() ||
        !win_util::ShouldUseVistaFrame()) {
      return SK_ColorWHITE;
    }
    return SK_ColorBLACK;
  }

  ConstrainedWindowImpl* container_;
  views::WindowDelegate* window_delegate_;

  scoped_ptr<views::WindowResources> resources_;

  gfx::Rect title_bounds_;
  gfx::Rect icon_bounds_;
  gfx::Rect client_bounds_;

  views::Button* close_button_;

  static void InitClass();

  // The font to be used to render the titlebar text.
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowNonClientView);
};

ChromeFont ConstrainedWindowNonClientView::title_font_;
static const int kWindowLeftSpacing = 5;
static const int kWindowControlsTopOffset = 1;
static const int kWindowControlsRightOffset = 4;
static const int kTitleTopOffset = 6;
static const int kTitleBottomSpacing = 5;
static const int kNoTitleTopSpacing = 8;
static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowHorizontalBorderSize = 5;
static const int kWindowVerticalBorderSize = 5;
static const int kWindowIconSize = 16;

static const SkColor kContentsBorderShadow = SkColorSetARGB(51, 0, 0, 0);
static const SkColor kContentsBorderColor = SkColorSetRGB(219, 235, 255);

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, public:

ConstrainedWindowNonClientView::ConstrainedWindowNonClientView(
    ConstrainedWindowImpl* container, TabContents* owner)
        : NonClientView(),
          container_(container),
          window_delegate_(NULL),
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

int ConstrainedWindowNonClientView::CalculateTitlebarHeight() const {
  int height;
  if (window_delegate_ && window_delegate_->ShouldShowWindowTitle()) {
    height = kTitleTopOffset + title_font_.height() + kTitleBottomSpacing;
  } else {
    height = kNoTitleTopSpacing;
  }

  return height;
}

gfx::Rect ConstrainedWindowNonClientView::CalculateWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int non_client_height = CalculateTitlebarHeight();
  gfx::Rect window_bounds = client_bounds;
  window_bounds.set_width(
      window_bounds.width() + 2 * kWindowHorizontalBorderSize);
  window_bounds.set_height(
      window_bounds.height() + non_client_height + kWindowVerticalBorderSize);
  window_bounds.set_x(
      std::max(0, window_bounds.x() - kWindowHorizontalBorderSize));
  window_bounds.set_y(std::max(0, window_bounds.y() - non_client_height));
  return window_bounds;
}

void ConstrainedWindowNonClientView::UpdateWindowTitle() {
  SchedulePaint(title_bounds_, false);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, views::NonClientView implementation:

gfx::Rect ConstrainedWindowNonClientView::CalculateClientAreaBounds(
    int width,
    int height) const {
  int non_client_height = CalculateTitlebarHeight();
  return gfx::Rect(kWindowHorizontalBorderSize, non_client_height,
      std::max(0, width - (2 * kWindowHorizontalBorderSize)),
      std::max(0, height - non_client_height - kWindowVerticalBorderSize));
}

gfx::Size ConstrainedWindowNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  // This is only used for truly constrained windows, which does not include
  // popups generated from a user gesture since those are detached immediately.
  gfx::Rect window_bounds =
      CalculateWindowBoundsForClientBounds(gfx::Rect(0, 0, width, height));
  return window_bounds.size();
}

CPoint ConstrainedWindowNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(icon_bounds_.x(), icon_bounds_.bottom());
  MapWindowPoints(container_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int ConstrainedWindowNonClientView::NonClientHitTest(const gfx::Point& point) {
  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = container_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  // Then see if the point is within any of the window controls.
  gfx::Rect button_bounds =
      close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTCLOSE;
  if (icon_bounds_.Contains(point))
    return HTSYSMENU;

  component = GetHTComponentForFrame(point, kResizeAreaSize,
                                     kResizeAreaCornerSize,
                                     kResizeAreaNorthSize,
                                     window_delegate_->CanResize());
  if (component == HTNOWHERE) {
    // Finally fall back to the caption.
    if (bounds().Contains(point))
      component = HTCAPTION;
    // Otherwise, the point is outside the window's bounds.
  }
  return component;
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

void ConstrainedWindowNonClientView::ResetWindowControls() {
  // We have no window controls to reset.
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, views::View implementation:

void ConstrainedWindowNonClientView::Paint(ChromeCanvas* canvas) {
  PaintFrameBorder(canvas);
  PaintTitleBar(canvas);
}

void ConstrainedWindowNonClientView::Layout() {
  gfx::Size ps;

  ps = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - ps.width() - kWindowControlsRightOffset,
                           kWindowControlsTopOffset, ps.width(), ps.height());

  int titlebar_height = CalculateTitlebarHeight();
  if (window_delegate_) {
    if (window_delegate_->ShouldShowWindowTitle()) {
      int spacing = kWindowLeftSpacing;
      int title_right = close_button_->x() - spacing;
      int title_left = icon_bounds_.right() + spacing;
      title_bounds_.SetRect(title_left, kTitleTopOffset,
                            title_right - title_left, title_font_.height());

      // Center the icon within the vertical bounds of the title if the title
      // is taller.
      int delta_y = title_bounds_.height() - icon_bounds_.height();
      if (delta_y > 0)
        icon_bounds_.set_y(title_bounds_.y() + static_cast<int>(delta_y / 2));
    }
  }

  client_bounds_ = CalculateClientAreaBounds(width(), height());
  container_->client_view()->SetBounds(client_bounds_);
}

gfx::Size ConstrainedWindowNonClientView::GetPreferredSize() {
  gfx::Size prefsize = container_->client_view()->GetPreferredSize();
  prefsize.Enlarge(2 * kWindowHorizontalBorderSize,
                   CalculateTitlebarHeight() +
                       kWindowVerticalBorderSize);
  return prefsize;
}

void ConstrainedWindowNonClientView::ViewHierarchyChanged(bool is_add,
                                                          View *parent,
                                                          View *child) {
  if (is_add && GetWidget()) {
    // Add our Client View as we are added to the Container so that if we are
    // subsequently resized all the parent-child relationships are established.
    if (is_add && GetWidget() && child == this)
      AddChildView(container_->client_view());
  }
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

void ConstrainedWindowNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_left_corner = resources_->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      resources_->GetPartBitmap(FRAME_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = resources_->GetPartBitmap(FRAME_TOP_CENTER);
  SkBitmap* right_edge = resources_->GetPartBitmap(FRAME_RIGHT_SIDE);
  SkBitmap* left_edge = resources_->GetPartBitmap(FRAME_LEFT_SIDE);
  SkBitmap* bottom_left_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = resources_->GetPartBitmap(FRAME_BOTTOM_CENTER);

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(
      *top_right_corner, width() - top_right_corner->width(), 0);

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

  // Contents Border.
  gfx::Rect border_bounds = client_bounds_;
  border_bounds.Inset(-2, -2);
  canvas->FillRectInt(kContentsBorderShadow, border_bounds.x(),
                      border_bounds.y(), border_bounds.width(),
                      border_bounds.height());

  border_bounds.Inset(1, 1);
  canvas->FillRectInt(kContentsBorderColor, border_bounds.x(),
                      border_bounds.y(), border_bounds.width(),
                      border_bounds.height());
}

void ConstrainedWindowNonClientView::PaintTitleBar(ChromeCanvas* canvas) {
  if (!window_delegate_)
    return;

  if (window_delegate_->ShouldShowWindowTitle()) {
    PaintWindowTitle(canvas);
  }
}

void ConstrainedWindowNonClientView::PaintWindowTitle(ChromeCanvas* canvas) {
  int title_x = MirroredLeftPointForRect(title_bounds_);
  canvas->DrawStringInt(container_->GetWindowTitle(), title_font_,
                        GetTitleColor(), title_x, title_bounds_.y(),
                        title_bounds_.width(), title_bounds_.height());
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
  NotificationService::current()->Notify(NOTIFY_CWINDOW_CLOSED,
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
  non_client_view()->set_window_delegate(window_delegate());
  CustomFrameWindow::Init(owner_->GetContainerHWND(), initial_bounds);
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
