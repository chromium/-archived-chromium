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

#include "chrome/browser/views/constrained_window_impl.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/constrained_window_animation.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/web_app.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/gfx/url_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/hwnd_view.h"
#include "generated_resources.h"
#include "net/base/net_util.h"

namespace ChromeViews {
class ClientView;
}

////////////////////////////////////////////////////////////////////////////////
// WindowResources

// An enumeration of bitmap resources used by this window.
enum FramePartBitmap {
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

class WindowResources {
 public:
  virtual ~WindowResources() {}
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part_id) const = 0;
  virtual const ChromeFont& GetTitleFont() const = 0;
  virtual SkColor GetTitleColor() const = 0;
};

class XPWindowResources : public WindowResources {
 public:
  XPWindowResources() {
    InitClass();
  }
  virtual ~XPWindowResources() {}

  virtual SkBitmap* GetPartBitmap(FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }
  virtual const ChromeFont& GetTitleFont() const { return title_font_; }
  virtual SkColor GetTitleColor() const { return SK_ColorWHITE; }

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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1);
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(XPWindowResources);
};

class VistaWindowResources : public WindowResources {
 public:
  VistaWindowResources() {
    InitClass();
  }
  virtual ~VistaWindowResources() {}

  virtual SkBitmap* GetPartBitmap(FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }
  virtual const ChromeFont& GetTitleFont() const { return title_font_; }
  virtual SkColor GetTitleColor() const { return SK_ColorBLACK; }

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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1);
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(VistaWindowResources);
};

class OTRWindowResources : public WindowResources {
 public:
  OTRWindowResources() {
    InitClass();
  }
  virtual ~OTRWindowResources() {}

  virtual SkBitmap* GetPartBitmap(FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }
  virtual const ChromeFont& GetTitleFont() const { return title_font_; }
  virtual SkColor GetTitleColor() const { return SK_ColorWHITE; }

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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1);
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(OTRWindowResources);
};

SkBitmap* XPWindowResources::bitmaps_[];
ChromeFont XPWindowResources::title_font_;
SkBitmap* VistaWindowResources::bitmaps_[];
ChromeFont VistaWindowResources::title_font_;
SkBitmap* OTRWindowResources::bitmaps_[];
ChromeFont OTRWindowResources::title_font_;

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView

class ConstrainedWindowNonClientView
    : public ChromeViews::CustomFrameWindow::NonClientView,
      public ChromeViews::BaseButton::ButtonListener,
      public LocationBarView::Delegate,
      public Task {
 public:
  ConstrainedWindowNonClientView(ConstrainedWindowImpl* container,
                                 TabContents* owner);
  virtual ~ConstrainedWindowNonClientView();

  // Calculates the pixel height of the titlebar
  int CalculateTitlebarHeight() const;

  // Calculates the pixel height of all pieces of a window that are
  // not part of the webcontent display area.
  int CalculateNonClientHeight(bool with_url_field) const;
  gfx::Rect CalculateWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds,
      bool with_url_field) const;
  void UpdateWindowIcon();
  void UpdateWindowTitle();

  void set_window_delegate(ChromeViews::WindowDelegate* window_delegate) {
    window_delegate_ = window_delegate;
  }

  // Changes whether we display a throbber or the current favicon and
  // forces a repaint of the titlebar.
  void SetShowThrobber(bool show_throbber);

  // Overridden from ChromeViews::CustomFrameWindow::NonClientView:
  virtual void Init(ChromeViews::ClientView* client_view);
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int HitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);

  // Overridden from ChromeViews::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from ChromeViews::BaseButton::ButtonListener:
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

  // Overridden from LocationBarView::Delegate:
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

  // Overridden from Task:
  virtual void Run();

  // Updates the current throbber animation frame; called from the
  // overloaded Run() and from SetShowThrobber().
  void UpdateThrobber();

  // Whether we should display the throbber instead of the favicon.
  bool should_show_throbber() const {
    return show_throbber_ && current_throbber_frame_ != -1;
  }

  // Paints different parts of the window to the incoming canvas.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintThrobber(ChromeCanvas* canvas);
  void PaintFavicon(ChromeCanvas* canvas);
  void PaintWindowTitle(ChromeCanvas* canvas);

  void UpdateLocationBar();
  bool ShouldDisplayURLField() const;

  // The View that provides the background for the window, and optionally
  // dialog buttons. Note: the non-client view does _not_ own this view, the
  // container does.
  ChromeViews::ClientView* client_view_;

  ConstrainedWindowImpl* container_;
  ChromeViews::WindowDelegate* window_delegate_;

  scoped_ptr<WindowResources> resources_;

  gfx::Rect title_bounds_;
  gfx::Rect icon_bounds_;
  gfx::Rect client_bounds_;

  ChromeViews::Button* close_button_;

  LocationBarView* location_bar_;

  // Specialization of ToolbarModel to obtain selected NavigationController for
  // a constrained TabContents.
  class ConstrainedWindowToolbarModel : public ToolbarModel {
   public:
    ConstrainedWindowToolbarModel(ConstrainedWindowImpl* constrained_window)
      : constrained_window_(constrained_window) {
    }
    ~ConstrainedWindowToolbarModel() { }

   protected:
    virtual NavigationController* GetNavigationController() {
      TabContents* tab = constrained_window_->constrained_contents();
      return tab ? tab->controller() : NULL;
    }

   private:
    ConstrainedWindowImpl* constrained_window_;

    DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowToolbarModel);
  };

  // The model used for the states of the location bar.
  ConstrainedWindowToolbarModel toolbar_model_;

  // Whether we should display the animated throbber instead of the
  // favicon.
  bool show_throbber_;

  // The timer used to update frames for the throbber.
  scoped_ptr<Timer> throbber_animation_timer_;

  // The current index into the throbber image strip.
  int current_throbber_frame_;

  static void InitClass();

  // The default favicon we render when the page has none.
  static SkBitmap default_favicon_;

  // The throbber to display while a constrained window is loading.
  static SkBitmap throbber_frames_;

  // The number of animation frames in throbber_frames_.
  static int throbber_frame_count_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowNonClientView);
};

SkBitmap ConstrainedWindowNonClientView::default_favicon_;
SkBitmap ConstrainedWindowNonClientView::throbber_frames_;
int ConstrainedWindowNonClientView::throbber_frame_count_ = -1;
static const int kWindowIconLeftOffset = 5;
static const int kWindowIconTopOffset = 5;
static const int kWindowControlsTopOffset = 1;
static const int kWindowControlsRightOffset = 4;
static const int kTitleTopOffset = 6;
static const int kWindowIconTitleSpacing = 3;
static const int kTitleBottomSpacing = 5;
static const int kNoTitleTopSpacing = 8;
static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowHorizontalBorderSize = 5;
static const int kWindowVerticalBorderSize = 5;
static const int kWindowIconSize = 16;

// How much wider or shorter the location bar is relative to the client area.
static const int kLocationBarOffset = 2;
// Spacing between the location bar and the content area.
static const int kLocationBarSpacing = 1;

static const SkColor kContentsBorderShadow = SkColorSetARGB(51, 0, 0, 0);
static const SkColor kContentsBorderColor = SkColorSetRGB(219, 235, 255);

static const int kThrobberFrameTimeMs = 30;

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, public:

ConstrainedWindowNonClientView::ConstrainedWindowNonClientView(
    ConstrainedWindowImpl* container, TabContents* owner)
        : container_(container),
          window_delegate_(NULL),
          close_button_(new ChromeViews::Button),
          location_bar_(NULL),
          show_throbber_(false),
          current_throbber_frame_(-1),
          toolbar_model_(container) {
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

  close_button_->SetImage(ChromeViews::Button::BS_NORMAL,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(ChromeViews::Button::BS_HOT,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(ChromeViews::Button::BS_PUSHED,
      resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_CENTER,
                                   ChromeViews::Button::ALIGN_MIDDLE);
  close_button_->SetListener(this, 0);
  AddChildView(close_button_);

  throbber_animation_timer_.reset(
      new Timer(kThrobberFrameTimeMs, this, true));

  // Note: we don't need for a controller because no input event will be ever
  // processed from a constrained window.
  location_bar_ = new LocationBarView(owner->profile(),
                                      NULL,
                                      &toolbar_model_,
                                      this,
                                      true);
  AddChildView(location_bar_);
}

ConstrainedWindowNonClientView::~ConstrainedWindowNonClientView() {
  MessageLoop::current()->timer_manager()->StopTimer(
      throbber_animation_timer_.get());
}

void ConstrainedWindowNonClientView::UpdateLocationBar() {
  if (ShouldDisplayURLField()) {
    std::wstring url_spec;
    TabContents* tab = container_->constrained_contents();
    url_spec = gfx::ElideUrl(tab->GetURL(),
        ChromeFont(),
        0,
        tab->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
    std::wstring ev_text, ev_tooltip_text;
    tab->GetSSLEVText(&ev_text, &ev_tooltip_text),
    location_bar_->Update(NULL);
  }
}

bool ConstrainedWindowNonClientView::ShouldDisplayURLField() const {
  // If the dialog is not fully initialized, default to showing the URL field.
  if (!container_ || !container_->owner() || !container_->owner()->delegate())
    return true;

  return !container_->is_dialog() &&
      container_->owner()->delegate()->ShouldDisplayURLField();
}

int ConstrainedWindowNonClientView::CalculateTitlebarHeight() const {
  int height;
  if (window_delegate_ && window_delegate_->ShouldShowWindowTitle()) {
    height = kTitleTopOffset + resources_->GetTitleFont().height() +
        kTitleBottomSpacing;
  } else {
    height = kNoTitleTopSpacing;
  }

  return height;
}

int ConstrainedWindowNonClientView::CalculateNonClientHeight(
    bool with_url_field) const {
  int r = CalculateTitlebarHeight();

  if (with_url_field) {
    CSize s;
    location_bar_->GetPreferredSize(&s);
    r += s.cy;
  }
  return r;
}

gfx::Rect ConstrainedWindowNonClientView::CalculateWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds,
    bool with_url_field) const {
  int non_client_height = CalculateNonClientHeight(with_url_field);
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

void ConstrainedWindowNonClientView::UpdateWindowIcon() {
  SchedulePaint(icon_bounds_.ToRECT(), false);
}

void ConstrainedWindowNonClientView::UpdateWindowTitle() {
  SchedulePaint(title_bounds_.ToRECT(), false);
  UpdateLocationBar();
}

void ConstrainedWindowNonClientView::SetShowThrobber(bool show_throbber) {
  show_throbber_ = show_throbber;

  TimerManager* tm = MessageLoop::current()->timer_manager();
  Timer* timer = throbber_animation_timer_.get();
  if (show_throbber) {
    if (!tm->IsTimerRunning(timer))
      tm->ResetTimer(timer);
  } else {
    if (tm->IsTimerRunning(timer)) {
      UpdateThrobber();
      tm->StopTimer(timer);
    }
  }
}

void ConstrainedWindowNonClientView::Run() {
  UpdateThrobber();
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView,
//     ChromeViews::CustomFrameWindow::NonClientView implementation:

void ConstrainedWindowNonClientView::Init(
    ChromeViews::ClientView* client_view) {
  client_view_ = client_view;
  AddChildView(client_view_);
}

gfx::Rect ConstrainedWindowNonClientView::CalculateClientAreaBounds(
    int width,
    int height) const {
  int non_client_height = CalculateNonClientHeight(ShouldDisplayURLField());
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
      CalculateWindowBoundsForClientBounds(gfx::Rect(0, 0, width, height),
                                           ShouldDisplayURLField());
  return window_bounds.size();
}

CPoint ConstrainedWindowNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(icon_bounds_.x(), icon_bounds_.bottom());
  MapWindowPoints(container_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

int ConstrainedWindowNonClientView::HitTest(const gfx::Point& point) {
  CRect bounds;
  CPoint test_point = point.ToPOINT();

  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  if (client_view_->PointIsInSizeBox(point))
    return HTBOTTOMRIGHT;

  // Then see if it's within the client area.
  if (client_view_) {
    client_view_->GetBounds(&bounds);
    if (bounds.PtInRect(test_point))
      return HTCLIENT;
  }

  // Then see if the point is within any of the window controls.
  close_button_->GetBounds(&bounds);
  if (bounds.PtInRect(test_point))
    return HTCLOSE;
  bounds = icon_bounds_.ToRECT();
  if (bounds.PtInRect(test_point))
    return HTSYSMENU;

  // Then see if the point is within the resize boundaries.
  int width = GetWidth();
  int height = GetHeight();
  int component = HTNOWHERE;
  if (point.x() < kResizeAreaSize) {
    if (point.y() < kResizeAreaCornerSize) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height - kResizeAreaCornerSize)) {
      component = HTBOTTOMLEFT;
    } else {
      component = HTLEFT;
    }
  } else if (point.x() < kResizeAreaCornerSize) {
    if (point.y() < kResizeAreaNorthSize) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height - kResizeAreaSize)) {
      component = HTBOTTOMLEFT;
    }
  } else if (point.x() >= (width - kResizeAreaSize)) {
    if (point.y() < kResizeAreaCornerSize) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height - kResizeAreaCornerSize)) {
      component = HTBOTTOMRIGHT;
    } else if (point.x() >= (width - kResizeAreaSize)) {
      component = HTRIGHT;
    }
  } else if (point.x() >= (width - kResizeAreaCornerSize)) {
    if (point.y() < kResizeAreaNorthSize) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height - kResizeAreaSize)) {
      component = HTBOTTOMRIGHT;
    }
  } else if (point.y() < kResizeAreaNorthSize) {
    component = HTTOP;
  } else if (point.y() >= (height - kResizeAreaSize)) {
    component = HTBOTTOM;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  if (component != HTNOWHERE) {
    if (window_delegate_ && !window_delegate_->CanResize()) {
      return HTBORDER;
    }
    return component;
  }

  // Finally fall back to the caption.
  GetBounds(&bounds);
  if (bounds.PtInRect(test_point))
    return HTCAPTION;
  // The point is outside the window's bounds.
  return HTNOWHERE;
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
// ConstrainedWindowNonClientView, ChromeViews::View implementation:

void ConstrainedWindowNonClientView::Paint(ChromeCanvas* canvas) {
  PaintFrameBorder(canvas);
  PaintTitleBar(canvas);
}

void ConstrainedWindowNonClientView::Layout() {
  bool should_display_url_field = false;
  if (location_bar_) {
    should_display_url_field = ShouldDisplayURLField();
    location_bar_->SetVisible(should_display_url_field);
  }

  int location_bar_height = 0;
  CSize ps;
  if (should_display_url_field) {
    location_bar_->GetPreferredSize(&ps);
    location_bar_height = ps.cy;
  }

  close_button_->GetPreferredSize(&ps);
  close_button_->SetBounds(GetWidth() - ps.cx - kWindowControlsRightOffset,
                           kWindowControlsTopOffset, ps.cx, ps.cy);

  int titlebar_height = CalculateTitlebarHeight();
  if (window_delegate_) {
    int icon_y = (titlebar_height - kWindowIconSize) / 2;
    icon_bounds_.SetRect(kWindowIconLeftOffset, icon_y, 0, 0);
    if (window_delegate_->ShouldShowWindowIcon()) {
      icon_bounds_.set_width(kWindowIconSize);
      icon_bounds_.set_height(kWindowIconSize);
    }

    if (window_delegate_->ShouldShowWindowTitle()) {
      int spacing = window_delegate_->ShouldShowWindowIcon() ?
          kWindowIconTitleSpacing : 0;
      int title_right = close_button_->GetX() - spacing;
      int title_left = icon_bounds_.right() + spacing;
      title_bounds_.SetRect(title_left, kTitleTopOffset,
                            title_right - title_left,
                            resources_->GetTitleFont().height());
    }
  }

  client_bounds_ = CalculateClientAreaBounds(GetWidth(), GetHeight());
  if (should_display_url_field) {
    location_bar_->SetBounds(client_bounds_.x() - kLocationBarOffset,
                             client_bounds_.y() - location_bar_height -
                             kLocationBarSpacing,
                             client_bounds_.width() + kLocationBarOffset * 2,
                             location_bar_height);
    location_bar_->Layout();
  }
  if (client_view_)
    client_view_->SetBounds(client_bounds_.ToRECT());
}

void ConstrainedWindowNonClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  if (client_view_) {
    client_view_->GetPreferredSize(out);
    out->cx += 2 * kWindowHorizontalBorderSize;
    out->cy += CalculateNonClientHeight(ShouldDisplayURLField()) +
        kWindowVerticalBorderSize;
  }
}

void ConstrainedWindowNonClientView::ViewHierarchyChanged(bool is_add,
                                                          View *parent,
                                                          View *child) {
  if (is_add && location_bar_ && GetViewContainer() &&
      !(location_bar_->IsInitialized())) {
    location_bar_->Init();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, ChromeViews::BaseButton::Button
//     implementation:

void ConstrainedWindowNonClientView::ButtonPressed(
    ChromeViews::BaseButton* sender) {
  if (sender == close_button_)
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, LocationBarView::Delegate
//     implementation:
TabContents* ConstrainedWindowNonClientView::GetTabContents() {
  return container_->owner();
}

void ConstrainedWindowNonClientView::OnInputInProgress(bool in_progress) {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowNonClientView, private:

void ConstrainedWindowNonClientView::UpdateThrobber() {
  if (show_throbber_)
    current_throbber_frame_ = ++current_throbber_frame_ % throbber_frame_count_;
  else
    current_throbber_frame_ = -1;

  SchedulePaint();
}

void ConstrainedWindowNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
  int width = GetWidth();
  int height = GetHeight();

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
                       width - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(
      *top_right_corner, width - top_right_corner->width(), 0);

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

  if (window_delegate_->ShouldShowWindowIcon()) {
    if (should_show_throbber())
      PaintThrobber(canvas);
    else
      PaintFavicon(canvas);
  }
  if (window_delegate_->ShouldShowWindowTitle()) {
    PaintWindowTitle(canvas);
  }
}

void ConstrainedWindowNonClientView::PaintThrobber(ChromeCanvas* canvas) {
  int image_size = throbber_frames_.height();
  int image_offset = current_throbber_frame_ * image_size;
  canvas->DrawBitmapInt(throbber_frames_,
                        image_offset, 0, image_size, image_size,
                        icon_bounds_.x(), icon_bounds_.y(),
                        image_size, image_size,
                        false);
}

void ConstrainedWindowNonClientView::PaintFavicon(ChromeCanvas* canvas) {
  SkBitmap window_icon = window_delegate_->GetWindowIcon();
  if (window_icon.isNull())
    window_icon = default_favicon_;
  canvas->DrawBitmapInt(window_icon, 0, 0, window_icon.width(),
                        window_icon.height(), icon_bounds_.x(),
                        icon_bounds_.y(), icon_bounds_.width(),
                        icon_bounds_.height(), false);
}

void ConstrainedWindowNonClientView::PaintWindowTitle(ChromeCanvas* canvas) {
  canvas->DrawStringInt(container_->GetWindowTitle(),
                        resources_->GetTitleFont(),
                        resources_->GetTitleColor(), title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height());
}

// static
void ConstrainedWindowNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);

    throbber_frames_ = *rb.GetBitmapNamed(IDR_THROBBER);
    DCHECK(throbber_frames_.width() % throbber_frames_.height() == 0);
    throbber_frame_count_ =
        throbber_frames_.width() / throbber_frames_.height();

    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedTabContentsWindowDelegate

class ConstrainedTabContentsWindowDelegate
    : public ChromeViews::WindowDelegate {
 public:
  explicit ConstrainedTabContentsWindowDelegate(
      ConstrainedWindowImpl* window)
      : window_(window) {
  }

  // ChromeViews::WindowDelegate implementation:
  virtual bool CanResize() const {
    return true;
  }
  virtual std::wstring GetWindowTitle() const {
    TabContents* constrained_contents = window_->constrained_contents();
    if (constrained_contents)
      return constrained_contents->GetTitle();

    return std::wstring();
  }
  virtual bool ShouldShowWindowIcon() const {
    return true;
  }
  virtual SkBitmap GetWindowIcon() {
    TabContents* constrained_contents = window_->constrained_contents();
    if (constrained_contents)
      return constrained_contents->GetFavIcon();

    return SkBitmap();
  }

 private:
  ConstrainedWindowImpl* window_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedTabContentsWindowDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, public:

// The space (in pixels) between minimized pop-ups stacked horizontally and
// vertically.
static const int kPopupRepositionOffset = 5;
static const int kConstrainedWindowEdgePadding = 10;

ConstrainedWindowImpl::ConstrainedWindowImpl(TabContents* owner)
    : CustomFrameWindow(new ConstrainedWindowNonClientView(this, owner)),
      owner_(owner),
      constrained_contents_(NULL),
      focus_restoration_disabled_(false),
      is_dialog_(false),
      titlebar_visibility_(0.0),
      contents_container_(NULL) {
  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION |
                   WS_THICKFRAME | WS_SYSMENU);
  set_focus_on_creation(false);
}

ConstrainedWindowImpl::~ConstrainedWindowImpl() {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, ConstrainedWindow implementation:

ConstrainedWindowNonClientView* ConstrainedWindowImpl::non_client_view() {
  return static_cast<ConstrainedWindowNonClientView*>(non_client_view_);
}

void ConstrainedWindowImpl::ActivateConstrainedWindow() {
  if (CanDetach()) {
    // Detachable pop-ups are torn out as soon as the window is activated.
    Detach();
    return;
  }

  // Other pop-ups are simply moved to the front of the z-order.
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  // Store the focus of our parent focus manager so we can restore it when we
  // close.
  ChromeViews::FocusManager* focus_manager =
      ChromeViews::FocusManager::GetFocusManager(GetHWND());
  DCHECK(focus_manager);
  focus_manager = focus_manager->GetParentFocusManager();
  if (focus_manager) {
    // We could not have a parent focus manager if the ConstrainedWindow is
    // displayed in a tab that is not currently selected.
    // TODO(jcampan): we should store the ConstrainedWindow active events in
    // that case and replay them when the WebContents becomes selected.
    focus_manager->StoreFocusedView();

    if (constrained_contents_) {
      // We contain another window, let's assume it knows how to process the
      // focus and let's focus it.
      // TODO(jcampan): so far this case is the WebContents case. We need to
      // better find whether the inner window should get focus.
      ::SetFocus(constrained_contents_->GetContainerHWND());
    } else {
      // Give our window the focus so we get keyboard messages.
      ::SetFocus(GetHWND());
    }
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

void ConstrainedWindowImpl::ResizeConstrainedWindow(int width, int height) {
  gfx::Size window_size =
      non_client_view_->CalculateWindowSizeForClientSize(width, height);
  ::SetWindowPos(GetHWND(), NULL, 0, 0, window_size.width(),
                 window_size.height(),
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

void ConstrainedWindowImpl::RepositionConstrainedWindowTo(
    const gfx::Point& anchor_point) {
  anchor_point_ = anchor_point;
  ResizeConstrainedTitlebar();
}

bool ConstrainedWindowImpl::IsSuppressedConstrainedWindow() const {
  return !is_dialog_;
}

void ConstrainedWindowImpl::WasHidden() {
  if (constrained_contents_)
    constrained_contents_->WasHidden();
}

void ConstrainedWindowImpl::DidBecomeSelected() {
  if (constrained_contents_)
    constrained_contents_->DidBecomeSelected();
}

std::wstring ConstrainedWindowImpl::GetWindowTitle() const {
  // TODO(beng): (http://b/1085485) Need to decide if we want to append the app
  //             name to all constrained windows or just web renderers.
  std::wstring page_title;
  if (window_delegate())
    page_title = window_delegate()->GetWindowTitle();

  std::wstring display_title;
  if (page_title.empty()) {
    display_title = l10n_util::GetString(IDS_PRODUCT_NAME);
  } else {
    display_title = l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT,
                                          page_title);
  }
  return display_title;
}

void ConstrainedWindowImpl::UpdateWindowTitle() {
  UpdateUI(TabContents::INVALIDATE_TITLE);
}

const gfx::Rect& ConstrainedWindowImpl::GetCurrentBounds() const {
  return current_bounds_;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, TabContentsDelegate implementation:

void ConstrainedWindowImpl::NavigationStateChanged(
    const TabContents* source,
    unsigned int changed_flags) {
  UpdateUI(changed_flags);
}

void ConstrainedWindowImpl::ReplaceContents(TabContents* source,
                                            TabContents* new_contents) {
  source->set_delegate(NULL);

  constrained_contents_ = new_contents;
  constrained_contents_->set_delegate(this);
  UpdateUI(TabContents::INVALIDATE_EVERYTHING);
}

void ConstrainedWindowImpl::AddNewContents(TabContents* source,
                                           TabContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  // Pass this to the delegate, since we can't open new tabs in the Constrained
  // Window, they are sent up to the browser to open as new tabs.
  owner_->AddNewContents(
      this, new_contents, disposition, initial_pos, user_gesture);
}

void ConstrainedWindowImpl::ActivateContents(TabContents* contents) {
  // Ask the delegate's (which is a TabContents) own TabContentsDelegate to
  // activate itself...
  owner_->delegate()->ActivateContents(owner_);

  // Set as the foreground constrained window.
  ActivateConstrainedWindow();
}

void ConstrainedWindowImpl::OpenURLFromTab(TabContents* source,
                                           const GURL& url,
                                           WindowOpenDisposition disposition,
                                           PageTransition::Type transition) {
  owner_->OpenURL(this, url, disposition, transition);
}

void ConstrainedWindowImpl::LoadingStateChanged(TabContents* source) {
  // TODO(beng): (http://b/1085543) Implement a throbber for the Constrained
  //             Window.
  UpdateUI(TabContents::INVALIDATE_EVERYTHING);
  non_client_view()->SetShowThrobber(source->is_loading());
}

void ConstrainedWindowImpl::NavigateToPage(TabContents* source,
                                           const GURL& url,
                                           PageTransition::Type transition) {
  UpdateUI(TabContents::INVALIDATE_EVERYTHING);
}

void ConstrainedWindowImpl::SetTitlebarVisibilityPercentage(double percentage) {
  titlebar_visibility_ = percentage;
  ResizeConstrainedTitlebar();
}

void ConstrainedWindowImpl::StartSuppressedAnimation() {
  animation_.reset(new ConstrainedWindowAnimation(this));
  animation_->Start();
}

void ConstrainedWindowImpl::CloseContents(TabContents* source) {
  Close();
}

void ConstrainedWindowImpl::MoveContents(TabContents* source,
                                         const gfx::Rect& pos) {
  if (!IsSuppressedConstrainedWindow())
    SetWindowBounds(pos);
}

bool ConstrainedWindowImpl::IsPopup(TabContents* source) {
  return true;
}

TabContents* ConstrainedWindowImpl::GetConstrainingContents(
    TabContents* source) {
  return owner_;
}

void ConstrainedWindowImpl::ToolbarSizeChanged(TabContents* source,
                                             bool finished) {
  // We don't control the layout of anything that could be animating,
  // so do nothing.
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, private:

void ConstrainedWindowImpl::ResizeConstrainedTitlebar() {
  DCHECK(constrained_contents_)
      << "ResizeConstrainedTitlebar() is only valid for web popups";
  // If we represent a web popup and we were not opened as the result of a
  // user gesture, we override the position specified in |initial_bounds| to
  // place ourselves at the bottom right of the parent HWND.
  CRect this_bounds;
  GetClientRect(&this_bounds);

  // First determine the height of the title bar of a constrained window, so
  // that we can offset by that much vertically if necessary...
  int titlebar_height = non_client_view()->CalculateTitlebarHeight();

  int visible_titlebar_pixels =
      static_cast<int>(titlebar_height * titlebar_visibility_);

  int x = anchor_point_.x() - this_bounds.Width();
  int y = anchor_point_.y() - visible_titlebar_pixels;
  SetWindowPos(NULL, x, y, this_bounds.Width(), visible_titlebar_pixels,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ConstrainedWindowImpl::InitAsDialog(
    const gfx::Rect& initial_bounds,
    ChromeViews::View* contents_view,
    ChromeViews::WindowDelegate* window_delegate) {
  is_dialog_ = true;
  non_client_view()->set_window_delegate(window_delegate);
  CustomFrameWindow::Init(owner_->GetContainerHWND(), initial_bounds,
                          contents_view, window_delegate);
  ActivateConstrainedWindow();
}

void ConstrainedWindowImpl::InitWindowForContents(
    TabContents* constrained_contents) {
  constrained_contents_ = constrained_contents;
  constrained_contents_->set_delegate(this);
  contents_container_ = new ChromeViews::HWNDView;
  contents_window_delegate_.reset(
      new ConstrainedTabContentsWindowDelegate(this));

  non_client_view()->set_window_delegate(contents_window_delegate_.get());
}

void ConstrainedWindowImpl::InitSizeForContents(
    const gfx::Rect& initial_bounds) {
  CustomFrameWindow::Init(owner_->GetContainerHWND(), initial_bounds,
                          contents_container_, contents_window_delegate_.get());
  contents_container_->Attach(constrained_contents_->GetContainerHWND());

  constrained_contents_->SizeContents(
      gfx::Size(contents_container_->GetWidth(),
                contents_container_->GetHeight()));
  current_bounds_ = initial_bounds;

  // Note that this is HWND_TOP, not HWND_TOPMOST... this is important
  // because otherwise the window will not be visible on top of the
  // RenderWidgetHostView!
  win_util::SetChildBounds(GetHWND(), GetParent(), HWND_TOP, initial_bounds,
                           kConstrainedWindowEdgePadding, 0);
}

bool ConstrainedWindowImpl::CanDetach() const {
  // Constrained TabContentses can be detached, dialog boxes can't.
  return constrained_contents_ ? true : false;
}

void ConstrainedWindowImpl::Detach() {
  DCHECK(CanDetach());
  // Tell the container not to restore focus to whatever view was focused last,
  // since this will interfere with the new window activation in the case where
  // a constrained window is destroyed by being detached.
  focus_restoration_disabled_ = true;

  // Detach the HWND immediately.
  contents_container_->Detach();
  contents_container_ = NULL;

  // To try and create as seamless as possible a popup experience, web pop-ups
  // are automatically detached when the user interacts with them. We can
  // dial this back if we feel this is too much.

  // The detached contents "should" be re-parented by the delegate's
  // DetachContents, but we clear the delegate pointing to us just in case.
  constrained_contents_->set_delegate(NULL);

  CRect bounds;
  ::GetWindowRect(constrained_contents_->GetContainerHWND(), &bounds);
  CPoint cursor_pos;
  ::GetCursorPos(&cursor_pos);
  gfx::Point screen_point(cursor_pos.x, cursor_pos.y);
  int frame_component = static_cast<int>(OnNCHitTest(screen_point.ToPOINT()));
  owner_->DetachContents(this, constrained_contents_, gfx::Rect(bounds),
                         screen_point, frame_component);
  constrained_contents_ = NULL;
  Close();
}

void ConstrainedWindowImpl::SetWindowBounds(const gfx::Rect& bounds) {
  // Note: SetChildBounds ensures that the constrained window is constrained
  //       to the bounds of its parent, however there remains a bug where the
  //       window is positioned incorrectly when the outer window is opened on
  //       a monitor that has negative coords (e.g. secondary monitor to left
  //       of primary, see http://b/issue?id=967905.)
  gfx::Size window_size = non_client_view()->CalculateWindowSizeForClientSize(
      bounds.width(), bounds.height());

  current_bounds_ = bounds;
  current_bounds_.set_width(window_size.width());
  current_bounds_.set_height(window_size.height());
  win_util::SetChildBounds(GetHWND(), GetParent(), NULL, current_bounds_,
                           kConstrainedWindowEdgePadding, 0);
}

void ConstrainedWindowImpl::UpdateUI(unsigned int changed_flags) {
  if (changed_flags & TabContents::INVALIDATE_FAVICON)
    non_client_view()->UpdateWindowIcon();
  if (changed_flags & TabContents::INVALIDATE_TITLE)
    non_client_view()->UpdateWindowTitle();
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl, ChromeViews::HWNDViewContainer overrides:

void ConstrainedWindowImpl::OnDestroy() {
  // We do this here, rather than |Close|, since the window may be destroyed in
  // a way other than by some other component calling Close, e.g. by the native
  // window hierarchy closing. We are guaranteed to receive a WM_DESTROY
  // message regardless of how the window is closed.
  // Note that when we get this message, the focus manager of the
  // ConstrainedWindow has already been destroyed (by the processing of
  // WM_DESTROY in FocusManager).  So the FocusManager we retrieve here is the
  // parent one (the one from the top window).
  ChromeViews::FocusManager* focus_manager =
      ChromeViews::FocusManager::GetFocusManager(GetHWND());
  if (focus_manager) {
    // We may not have a focus manager if:
    // - we are hidden when closed (the TabContent would be detached).
    // - the tab has been closed and we are closed as a result.
    // TODO(jcampan): when hidden, we should modify the stored focus of the tab
    // so when it becomes visible again we retrieve the focus appropriately.
    if (!focus_restoration_disabled_)
      focus_manager->RestoreFocusedView();
  }

  // If we have a child TabContents, we need to unhook it here so that it is
  // not automatically WM_DESTROYed by virtue of the fact that it is part of
  // our Window hierarchy. Rather, it needs to be destroyed just like top level
  // TabContentses are: from OnMsgCloseACK in RenderWidgetHost. So we hide the
  // TabContents and sever the parent relationship now. Note the GetParent
  // check so that we don't hide and re-parent TabContentses that have been
  // detached and re-attached into a new top level browser window via a user
  // drag action.
  if (constrained_contents_ &&
      ::GetParent(constrained_contents_->GetContainerHWND()) == GetHWND()) {
    ::ShowWindow(constrained_contents_->GetContainerHWND(), SW_HIDE);
    ::SetParent(constrained_contents_->GetContainerHWND(), NULL);
  }

  // Make sure we call super so that it can do its cleanup.
  Window::OnDestroy();
}

void ConstrainedWindowImpl::OnFinalMessage(HWND window) {
  // Tell our constraining TabContents that we've gone so it can update its
  // list.
  owner_->WillClose(this);
  if (constrained_contents_) {
    constrained_contents_->CloseContents();
    constrained_contents_ = NULL;
  }

  HWNDViewContainer::OnFinalMessage(window);
}

void ConstrainedWindowImpl::OnGetMinMaxInfo(LPMINMAXINFO mm_info) {
  // Override this function to set the maximize area as the client area of the
  // containing window.
  CRect parent_rect;
  ::GetClientRect(GetParent(), &parent_rect);

  mm_info->ptMaxSize.x = parent_rect.Width();
  mm_info->ptMaxSize.y = parent_rect.Height();
  mm_info->ptMaxPosition.x = parent_rect.left;
  mm_info->ptMaxPosition.y = parent_rect.top;
}

LRESULT ConstrainedWindowImpl::OnMouseActivate(HWND window,
                                               UINT hittest_code,
                                               UINT message) {
  // We need to store this value before we call ActivateConstrainedWindow()
  // since the window may be detached and so this function will return false
  // afterwards.
  bool can_detach = CanDetach();

  // We only detach the window if the user clicked on the title bar. That
  // way, users can click inside the contents of legitimate popups obtained
  // with a mouse gesture.
  if (hittest_code == HTCAPTION) {
    ActivateConstrainedWindow();
  } else {
    // If the user did not click on the title bar, don't stop message
    // propagation.
    can_detach = false;
  }

  // If the popup can be detached, then we tell the parent window not to
  // activate since we will already have adjusted activation ourselves. We also
  // do _not_ eat the event otherwise the user will have to click again to
  // interact with the popup.
  return can_detach ? MA_NOACTIVATEANDEAT : MA_ACTIVATE;
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
void ConstrainedWindow::GenerateInitialBounds(
    const gfx::Rect& initial_bounds, TabContents* parent,
    gfx::Rect* window_bounds) {
  // Calculate desired window bounds.  Try to use the bounds of a
  // non-maximized browser window; this matches other browsers' behavior.
  //
  // NOTE: The downside here is that, if we open multiple constrained popups,
  // they'll all get the same window position, since WindowSizer uses the
  // "last active browser window"'s bounds.  Fixing this properly is hard,
  // since we'd have to tell the WindowSizer about the window we're opening
  // here, and figure out how the sizing memory and the clipping/offsetting
  // behvaiors below interact.
  std::wstring app_name;

  if (parent->delegate() && parent->delegate()->IsApplication() &&
      parent->AsWebContents() && parent->AsWebContents()->web_app()) {
    app_name = parent->AsWebContents()->web_app()->name();
  }
  bool maximized = false;
  gfx::Rect empty_bounds;
  WindowSizer::GetBrowserWindowBounds(app_name, empty_bounds,
                                      window_bounds, &maximized);
  if (initial_bounds.width() > 0)
    window_bounds->set_width(initial_bounds.width());
  if (initial_bounds.height() > 0)
    window_bounds->set_height(initial_bounds.height());

  // Map desired window bounds from screen coordinates to our parent's
  // coordinates.
  CPoint window_origin(window_bounds->origin().ToPOINT());
  MapWindowPoints(HWND_DESKTOP, parent->GetContainerHWND(), &window_origin,
                  1);
  window_bounds->set_origin(gfx::Point(window_origin));

  // Ensure some amount of the page is visible above and to the left of the
  // popup, so it doesn't cover the whole content area (we use 30 px).
  if (window_bounds->x() < 30)
    window_bounds->set_x(30);
  if (window_bounds->y() < 30)
    window_bounds->set_y(30);

  // Clip the desired coordinates so they fit within the content area.
  CRect parent_rect;
  ::GetClientRect(parent->GetContainerHWND(), &parent_rect);
  if (window_bounds->right() > parent_rect.right)
    window_bounds->set_width(parent_rect.Width() - window_bounds->x());
  if (window_bounds->bottom() > parent_rect.bottom)
    window_bounds->set_height(parent_rect.Height() - window_bounds->y());

  // Don't let the window become too small (we use a 60x30 minimum size).
  if (window_bounds->width() < 60)
    window_bounds->set_width(60);
  if (window_bounds->height() < 30)
    window_bounds->set_height(30);
}

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    const gfx::Rect& initial_bounds,
    ChromeViews::View* contents_view,
    ChromeViews::WindowDelegate* window_delegate) {
  ConstrainedWindowImpl* window = new ConstrainedWindowImpl(parent);
  window->InitAsDialog(initial_bounds, contents_view, window_delegate);
  return window;
}

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedPopup(
    TabContents* parent,
    const gfx::Rect& initial_bounds,
    TabContents* constrained_contents) {
  ConstrainedWindowImpl* window =
      new ConstrainedWindowImpl(parent);
  window->InitWindowForContents(constrained_contents);

  gfx::Rect window_bounds;
  if (initial_bounds.width() == 0 || initial_bounds.height() == 0) {
    GenerateInitialBounds(initial_bounds, parent, &window_bounds);
  } else {
    window_bounds = window->non_client_view()->
        CalculateWindowBoundsForClientBounds(
            initial_bounds,
            parent->delegate()->ShouldDisplayURLField());
  }

  window->InitSizeForContents(window_bounds);

  // This is a constrained popup window and thus we need to animate it in.
  window->StartSuppressedAnimation();

  return window;
}
