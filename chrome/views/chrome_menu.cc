// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/chrome_menu.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "base/base_drag_source.h"
#include "base/gfx/native_theme.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/timer.h"
#include "base/win_util.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/views/border.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_constants.h"
#include "chrome/views/widget_win.h"
#include "generated_resources.h"
#include "skia/ext/skia_utils_win.h"

#undef min
#undef max

using base::Time;
using base::TimeDelta;

// Margins between the top of the item and the label.
static const int kItemTopMargin = 3;

// Margins between the bottom of the item and the label.
static const int kItemBottomMargin = 4;

// Margins used if the menu doesn't have icons.
static const int kItemNoIconTopMargin = 1;
static const int kItemNoIconBottomMargin = 3;

// Margins between the left of the item and the icon.
static const int kItemLeftMargin = 4;

// Padding between the label and submenu arrow.
static const int kLabelToArrowPadding = 10;

// Padding between the arrow and the edge.
static const int kArrowToEdgePadding = 5;

// Padding between the icon and label.
static const int kIconToLabelPadding = 8;

// Padding between the gutter and label.
static const int kGutterToLabel = 5;

// Height of the scroll arrow.
// This goes up to 4 with large fonts, but this is close enough for now.
static const int kScrollArrowHeight = 3;

// Size of the check. This comes from the OS.
static int check_width;
static int check_height;

// Size of the submenu arrow. This comes from the OS.
static int arrow_width;
static int arrow_height;

// Width of the gutter. Only used if render_gutter is true.
static int gutter_width;

// Margins between the right of the item and the label.
static int item_right_margin;

// X-coordinate of where the label starts.
static int label_start;

// Height of the separator.
static int separator_height;

// Padding around the edges of the submenu.
static const int kSubmenuBorderSize = 3;

// Amount to inset submenus.
static const int kSubmenuHorizontalInset = 3;

// Delay, in ms, between when menus are selected are moused over and the menu
// appears.
static const int kShowDelay = 400;

// Amount of time from when the drop exits the menu and the menu is hidden.
static const int kCloseOnExitTime = 1200;

// Height of the drop indicator. This should be an event number.
static const int kDropIndicatorHeight = 2;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

// Whether or not the gutter should be rendered. The gutter is specific to
// Vista.
static bool render_gutter = false;

// Max width of a menu. There does not appear to be an OS value for this, yet
// both IE and FF restrict the max width of a menu.
static const int kMaxMenuWidth = 400;

// Period of the scroll timer (in milliseconds).
static const int kScrollTimerMS = 30;

// Preferred height of menu items. Reset every time a menu is run.
static int pref_menu_height;

// Are mnemonics shown? This is updated before the menus are shown.
static bool show_mnemonics;

using gfx::NativeTheme;

namespace views {

namespace {

// Returns the font menus are to use.
ChromeFont GetMenuFont() {
	NONCLIENTMETRICS metrics;
  win_util::GetNonClientMetrics(&metrics);

  HFONT font = CreateFontIndirect(&metrics.lfMenuFont);
  DLOG_ASSERT(font);
  return ChromeFont::CreateFont(font);
}

// Calculates all sizes that we can from the OS.
//
// This is invoked prior to Running a menu.
void UpdateMenuPartSizes(bool has_icons) {
  HDC dc = GetDC(NULL);
  RECT bounds = { 0, 0, 200, 200 };
  SIZE check_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, &bounds,
          TS_TRUE, &check_size) == S_OK) {
    check_width = check_size.cx;
    check_height = check_size.cy;
  } else {
    check_width = GetSystemMetrics(SM_CXMENUCHECK);
    check_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE arrow_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSUBMENU, MSM_NORMAL, &bounds,
          TS_TRUE, &arrow_size) == S_OK) {
    arrow_width = arrow_size.cx;
    arrow_height = arrow_size.cy;
  } else {
    // Sadly I didn't see a specify metrics for this.
    arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
    arrow_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE gutter_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPGUTTER, MSM_NORMAL, &bounds,
          TS_TRUE, &gutter_size) == S_OK) {
    gutter_width = gutter_size.cx;
    render_gutter = true;
  } else {
    gutter_width = 0;
    render_gutter = false;
  }

  SIZE separator_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSEPARATOR, MSM_NORMAL, &bounds,
          TS_TRUE, &separator_size) == S_OK) {
    separator_height = separator_size.cy;
  } else {
    separator_height = GetSystemMetrics(SM_CYMENU) / 2;
  }

  item_right_margin = kLabelToArrowPadding + arrow_width + kArrowToEdgePadding;

  if (has_icons) {
    label_start = kItemLeftMargin + check_width + kIconToLabelPadding;
  } else {
    // If there are no icons don't pad by the icon to label padding. This
    // makes us look close to system menus.
    label_start = kItemLeftMargin + check_width;
  }
  if (render_gutter)
    label_start += gutter_width + kGutterToLabel;

  ReleaseDC(NULL, dc);

  MenuItemView menu_item(NULL);
  menu_item.SetTitle(L"blah");  // Text doesn't matter here.
  pref_menu_height = menu_item.GetPreferredSize().height();
}

// Convenience for scrolling the view such that the origin is visible.
static void ScrollToVisible(View* view) {
  view->ScrollRectToVisible(0, 0, view->width(), view->height());
}

// MenuScrollTask --------------------------------------------------------------

// MenuScrollTask is used when the SubmenuView does not all fit on screen and
// the mouse is over the scroll up/down buttons. MenuScrollTask schedules
// itself with a RepeatingTimer. When Run is invoked MenuScrollTask scrolls
// appropriately.

class MenuScrollTask {
public:
  MenuScrollTask() : submenu_(NULL) {
    pixels_per_second_ = pref_menu_height * 20;
  }

  void Update(const MenuController::MenuPart& part) {
    if (!part.is_scroll()) {
      StopScrolling();
      return;
    }
    DCHECK(part.submenu);
    SubmenuView* new_menu = part.submenu;
    bool new_is_up = (part.type == MenuController::MenuPart::SCROLL_UP);
    if (new_menu == submenu_ && is_scrolling_up_ == new_is_up)
      return;

    start_scroll_time_ = Time::Now();
    start_y_ = part.submenu->GetVisibleBounds().y();
    submenu_ = new_menu;
    is_scrolling_up_ = new_is_up;

    if (!scrolling_timer_.IsRunning()) {
      scrolling_timer_.Start(TimeDelta::FromMilliseconds(kScrollTimerMS), this,
                             &MenuScrollTask::Run);
    }
  }

  void StopScrolling() {
    if (scrolling_timer_.IsRunning()) {
      scrolling_timer_.Stop();
      submenu_ = NULL;
    }
  }

  // The menu being scrolled. Returns null if not scrolling.
  SubmenuView* submenu() const { return submenu_; }

 private:
  void Run() {
    DCHECK(submenu_);
    gfx::Rect vis_rect = submenu_->GetVisibleBounds();
    const int delta_y = static_cast<int>(
        (Time::Now() - start_scroll_time_).InMilliseconds() *
        pixels_per_second_ / 1000);
    int target_y = start_y_;
    if (is_scrolling_up_)
      target_y = std::max(0, target_y - delta_y);
    else
      target_y = std::min(submenu_->height() - vis_rect.height(),
                          target_y + delta_y);
    submenu_->ScrollRectToVisible(vis_rect.x(), target_y, vis_rect.width(),
                                  vis_rect.height());
  }

  // SubmenuView being scrolled.
  SubmenuView* submenu_;

  // Direction scrolling.
  bool is_scrolling_up_;

  // Timer to periodically scroll.
  base::RepeatingTimer<MenuScrollTask> scrolling_timer_;

  // Time we started scrolling at.
  Time start_scroll_time_;

  // How many pixels to scroll per second.
  int pixels_per_second_;

  // Y-coordinate of submenu_view_ when scrolling started.
  int start_y_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuScrollTask);
};

// MenuScrollButton ------------------------------------------------------------

// MenuScrollButton is used for the scroll buttons when not all menu items fit
// on screen. MenuScrollButton forwards appropriate events to the
// MenuController.

class MenuScrollButton : public View {
 public:
  explicit MenuScrollButton(SubmenuView* host, bool is_up)
      : host_(host),
        is_up_(is_up),
        // Make our height the same as that of other MenuItemViews.
        pref_height_(pref_menu_height) {
  }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(kScrollArrowHeight * 2 - 1, pref_height_);
  }

  virtual bool CanDrop(const OSExchangeData& data) {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    return true;  // Always return true so that drop events are targeted to us.
  }

  virtual void OnDragEntered(const DropTargetEvent& event) {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragEnteredScrollButton(
        host_, is_up_);
  }

  virtual int OnDragUpdated(const DropTargetEvent& event) {
    return DragDropTypes::DRAG_NONE;
  }

  virtual void OnDragExited() {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragExitedScrollButton(host_);
  }

  virtual int OnPerformDrop(const DropTargetEvent& event) {
    return DragDropTypes::DRAG_NONE;
  }

  virtual void Paint(ChromeCanvas* canvas) {
    HDC dc = canvas->beginPlatformPaint();

    // The background.
    RECT item_bounds = { 0, 0, width(), height() };
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, MPI_NORMAL, false,
        &item_bounds);

    // Then the arrow.
    int x = width() / 2;
    int y = (height() - kScrollArrowHeight) / 2;
    int delta_y = 1;
    if (!is_up_) {
      delta_y = -1;
      y += kScrollArrowHeight;
    }
    SkColor arrow_color = color_utils::GetSysSkColor(COLOR_MENUTEXT);
    for (int i = 0; i < kScrollArrowHeight; ++i, --x, y += delta_y)
      canvas->FillRectInt(arrow_color, x, y, (i * 2) + 1, 1);

    canvas->endPlatformPaint();
  }

 private:
  // SubmenuView we were created for.
  SubmenuView* host_;

  // Direction of the button.
  bool is_up_;

  // Preferred height.
  int pref_height_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuScrollButton);
};

// MenuScrollView --------------------------------------------------------------

// MenuScrollView is a viewport for the SubmenuView. It's reason to exist is so
// that ScrollRectToVisible works.
//
// NOTE: It is possible to use ScrollView directly (after making it deal with
// null scrollbars), but clicking on a child of ScrollView forces the window to
// become active, which we don't want. As we really only need a fraction of
// what ScrollView does, we use a one off variant.

class MenuScrollView : public View {
 public:
  explicit MenuScrollView(View* child) {
    AddChildView(child);
  }

  virtual void ScrollRectToVisible(int x, int y, int width, int height) {
    // NOTE: this assumes we only want to scroll in the y direction.

    View* child = GetContents();
    // Convert y to view's coordinates.
    y -= child->y();
    gfx::Size pref = child->GetPreferredSize();
    // Constrain y to make sure we don't show past the bottom of the view.
    y = std::max(0, std::min(pref.height() - this->height(), y));
    child->SetY(-y);
  }

  // Returns the contents, which is the SubmenuView.
  View* GetContents() {
    return GetChildViewAt(0);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(MenuScrollView);
};

// MenuScrollViewContainer -----------------------------------------------------

// MenuScrollViewContainer contains the SubmenuView (through a MenuScrollView)
// and two scroll buttons. The scroll buttons are only visible and enabled if
// the preferred height of the SubmenuView is bigger than our bounds.
class MenuScrollViewContainer : public View {
 public:
  explicit MenuScrollViewContainer(SubmenuView* content_view) {
    scroll_up_button_ = new MenuScrollButton(content_view, true);
    scroll_down_button_ = new MenuScrollButton(content_view, false);
    AddChildView(scroll_up_button_);
    AddChildView(scroll_down_button_);

    scroll_view_ = new MenuScrollView(content_view);
    AddChildView(scroll_view_);

    set_border(Border::CreateEmptyBorder(
        kSubmenuBorderSize, kSubmenuBorderSize,
        kSubmenuBorderSize, kSubmenuBorderSize));
  }

  virtual void Paint(ChromeCanvas* canvas) {
    HDC dc = canvas->beginPlatformPaint();
    CRect bounds(0, 0, width(), height());
    NativeTheme::instance()->PaintMenuBackground(
        NativeTheme::MENU, dc, MENU_POPUPBACKGROUND, 0, &bounds);
    canvas->endPlatformPaint();
  }

  View* scroll_down_button() { return scroll_down_button_; }

  View* scroll_up_button() { return scroll_up_button_; }

  virtual void Layout() {
    gfx::Insets insets = GetInsets();
    int x = insets.left();
    int y = insets.top();
    int width = View::width() - insets.width();
    int content_height = height() - insets.height();
    if (!scroll_up_button_->IsVisible()) {
      scroll_view_->SetBounds(x, y, width, content_height);
      scroll_view_->Layout();
      return;
    }

    gfx::Size pref = scroll_up_button_->GetPreferredSize();
    scroll_up_button_->SetBounds(x, y, width, pref.height());
    content_height -= pref.height();

    const int scroll_view_y = y + pref.height();

    pref = scroll_down_button_->GetPreferredSize();
    scroll_down_button_->SetBounds(x, height() - pref.height() - insets.top(),
                                   width, pref.height());
    content_height -= pref.height();

    scroll_view_->SetBounds(x, scroll_view_y, width, content_height);
    scroll_view_->Layout();
  }

  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current) {
    gfx::Size content_pref = scroll_view_->GetContents()->GetPreferredSize();
    scroll_up_button_->SetVisible(content_pref.height() > height());
    scroll_down_button_->SetVisible(content_pref.height() > height());
  }

  virtual gfx::Size GetPreferredSize() {
    gfx::Size prefsize = scroll_view_->GetContents()->GetPreferredSize();
    gfx::Insets insets = GetInsets();
    prefsize.Enlarge(insets.width(), insets.height());
    return prefsize;
  }

 private:
  // The scroll buttons.
  View* scroll_up_button_;
  View* scroll_down_button_;

  // The scroll view.
  MenuScrollView* scroll_view_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuScrollViewContainer);
};

// MenuSeparator ---------------------------------------------------------------

// Renders a separator.

class MenuSeparator : public View {
 public:
  MenuSeparator() {
  }

  void Paint(ChromeCanvas* canvas) {
    // The gutter is rendered before the background.
    int start_x = 0;
    int start_y = height() / 3;
    HDC dc = canvas->beginPlatformPaint();
    if (render_gutter) {
      // If render_gutter is true, we're on Vista and need to render the
      // gutter, then indent the separator from the gutter.
      RECT gutter_bounds = { label_start - kGutterToLabel - gutter_width, 0, 0,
                              height() };
      gutter_bounds.right = gutter_bounds.left + gutter_width;
      NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL,
                                               &gutter_bounds);
      start_x = gutter_bounds.left + gutter_width;
      start_y = 0;
    }
    RECT separator_bounds = { start_x, start_y, width(), height() };
    NativeTheme::instance()->PaintMenuSeparator(dc, MENU_POPUPSEPARATOR,
                                                MPI_NORMAL, &separator_bounds);
    canvas->endPlatformPaint();
  }

  gfx::Size GetPreferredSize() {
    return gfx::Size(10, // Just in case we're the only item in a menu.
                     separator_height);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(MenuSeparator);
};

// MenuHostRootView ----------------------------------------------------------

// MenuHostRootView is the RootView of the window showing the menu.
// SubmenuView's scroll view is added as a child of MenuHostRootView.
// MenuHostRootView forwards relevant events to the MenuController.
//
// As all the menu items are owned by the root menu item, care must be taken
// such that when MenuHostRootView is deleted it doesn't delete the menu items.

class MenuHostRootView : public RootView {
 public:
  explicit MenuHostRootView(Widget* widget,
                            SubmenuView* submenu)
      : RootView(widget),
        submenu_(submenu),
        forward_drag_to_menu_controller_(true),
        suspend_events_(false) {
#ifdef DEBUG_MENU
  DLOG(INFO) << " new MenuHostRootView " << this;
#endif
  }

  virtual bool OnMousePressed(const MouseEvent& event) {
    if (suspend_events_)
      return true;

    forward_drag_to_menu_controller_ =
        ((event.x() < 0 || event.y() < 0 || event.x() >= width() ||
          event.y() >= height()) ||
         !RootView::OnMousePressed(event));
    if (forward_drag_to_menu_controller_)
      GetMenuController()->OnMousePressed(submenu_, event);
    return true;
  }

  virtual bool OnMouseDragged(const MouseEvent& event) {
    if (suspend_events_)
      return true;

    if (forward_drag_to_menu_controller_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << " MenuHostRootView::OnMouseDragged source=" << submenu_;
#endif
      GetMenuController()->OnMouseDragged(submenu_, event);
      return true;
    }
    return RootView::OnMouseDragged(event);
  }

  virtual void OnMouseReleased(const MouseEvent& event, bool canceled) {
    if (suspend_events_)
      return;

    RootView::OnMouseReleased(event, canceled);
    if (forward_drag_to_menu_controller_) {
      forward_drag_to_menu_controller_ = false;
      if (canceled) {
        GetMenuController()->Cancel(true);
      } else {
        GetMenuController()->OnMouseReleased(submenu_, event);
      }
    }
  }

  virtual void OnMouseMoved(const MouseEvent& event) {
    if (suspend_events_)
      return;

    RootView::OnMouseMoved(event);
    GetMenuController()->OnMouseMoved(submenu_, event);
  }

  virtual void ProcessOnMouseExited() {
    if (suspend_events_)
      return;

    RootView::ProcessOnMouseExited();
  }

  virtual bool ProcessMouseWheelEvent(const MouseWheelEvent& e) {
    // RootView::ProcessMouseWheelEvent forwards to the focused view. We don't
    // have a focused view, so we need to override this then forward to
    // the menu.
    return submenu_->OnMouseWheel(e);
  }

  void SuspendEvents() {
    suspend_events_ = true;
  }

 private:
  MenuController* GetMenuController() {
    return submenu_->GetMenuItem()->GetMenuController();
  }

  /// The SubmenuView we contain.
  SubmenuView* submenu_;

  // Whether mouse dragged/released should be forwarded to the MenuController.
  bool forward_drag_to_menu_controller_;

  // Whether events are suspended. If true, no events are forwarded to the
  // MenuController.
  bool suspend_events_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuHostRootView);
};

// MenuHost ------------------------------------------------------------------

// MenuHost is the window responsible for showing a single menu.
//
// Similar to MenuHostRootView, care must be taken such that when MenuHost is
// deleted, it doesn't delete the menu items. MenuHost is closed via a
// DelayedClosed, which avoids timing issues with deleting the window while
// capture or events are directed at it.

class MenuHost : public WidgetWin {
 public:
  MenuHost(SubmenuView* submenu)
      : closed_(false),
        submenu_(submenu),
        owns_capture_(false) {
    set_window_style(WS_POPUP);
    set_initial_class_style(
        (win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
        0 : CS_DROPSHADOW);
    is_mouse_down_ =
        ((GetKeyState(VK_LBUTTON) & 0x80) ||
         (GetKeyState(VK_RBUTTON) & 0x80) ||
         (GetKeyState(VK_MBUTTON) & 0x80) ||
         (GetKeyState(VK_XBUTTON1) & 0x80) ||
         (GetKeyState(VK_XBUTTON2) & 0x80));
    // Mouse clicks shouldn't give us focus.
    set_window_ex_style(WS_EX_TOPMOST | WS_EX_NOACTIVATE);
  }

  void Init(HWND parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool do_capture) {
    WidgetWin::Init(parent, bounds, true);
    SetContentsView(contents_view);
    // We don't want to take focus away from the hosting window.
    ShowWindow(SW_SHOWNA);
    owns_capture_ = do_capture;
    if (do_capture) {
      SetCapture();
      has_capture_ = true;
#ifdef DEBUG_MENU
      DLOG(INFO) << "Doing capture";
#endif
    }
  }

  virtual void Hide() {
    if (closed_) {
      // We're already closed, nothing to do.
      // This is invoked twice if the first time just hid us, and the second
      // time deleted Closed (deleted) us.
      return;
    }
    // The menus are freed separately, and possibly before the window is closed,
    // remove them so that View doesn't try to access deleted objects.
    static_cast<MenuHostRootView*>(GetRootView())->SuspendEvents();
    GetRootView()->RemoveAllChildViews(false);
    closed_ = true;
    ReleaseCapture();
    WidgetWin::Hide();
  }

  virtual void HideWindow() {
    // Make sure we release capture before hiding.
    ReleaseCapture();
    WidgetWin::Hide();
  }

  virtual void OnCaptureChanged(HWND hwnd) {
    WidgetWin::OnCaptureChanged(hwnd);
    owns_capture_ = false;
#ifdef DEBUG_MENU
    DLOG(INFO) << "Capture changed";
#endif
  }

  void ReleaseCapture() {
    if (owns_capture_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << "released capture";
#endif
      owns_capture_ = false;
      ::ReleaseCapture();
    }
  }

 protected:
  // Overriden to create MenuHostRootView.
  virtual RootView* CreateRootView() {
    return new MenuHostRootView(this, submenu_);
  }

  virtual void OnCancelMode() {
    if (!closed_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << "OnCanceMode, closing menu";
#endif
      submenu_->GetMenuItem()->GetMenuController()->Cancel(true);
    }
  }

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased() {
    return false;
  }

 private:
  // If true, we've been closed.
  bool closed_;

  // If true, we own the capture and need to release it.
  bool owns_capture_;

  // The view we contain.
  SubmenuView* submenu_;

  DISALLOW_EVIL_CONSTRUCTORS(MenuHost);
};

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  // ID used for EmptyMenuMenuItem.
  static const int kEmptyMenuItemViewID;

  EmptyMenuMenuItem(MenuItemView* parent) : MenuItemView(parent, 0, NORMAL) {
    SetTitle(l10n_util::GetString(IDS_MENU_EMPTY_SUBMENU));
    // Set this so that we're not identified as a normal menu item.
    SetID(kEmptyMenuItemViewID);
    SetEnabled(false);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(EmptyMenuMenuItem);
};

// static
const int EmptyMenuMenuItem::kEmptyMenuItemViewID =
    MenuItemView::kMenuItemViewID + 1;

}  // namespace

// SubmenuView ---------------------------------------------------------------

SubmenuView::SubmenuView(MenuItemView* parent)
    : parent_menu_item_(parent),
      host_(NULL),
      drop_item_(NULL),
      scroll_view_container_(NULL) {
  DCHECK(parent);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  SetParentOwned(false);
}

SubmenuView::~SubmenuView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();

  delete scroll_view_container_;
}

int SubmenuView::GetMenuItemCount() {
  int count = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID)
      count++;
  }
  return count;
}

MenuItemView* SubmenuView::GetMenuItemAt(int index) {
  for (int i = 0, count = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID &&
        count++ == index) {
      return static_cast<MenuItemView*>(GetChildViewAt(i));
    }
  }
  NOTREACHED();
  return NULL;
}

void SubmenuView::Layout() {
  // We're in a ScrollView, and need to set our width/height ourselves.
  View* parent = GetParent();
  if (!parent)
    return;
  SetBounds(x(), y(), parent->width(), GetPreferredSize().height());

  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int menu_item_width = width() - insets.width();
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size child_pref_size = child->GetPreferredSize();
    child->SetBounds(x, y, menu_item_width, child_pref_size.height());
    y += child_pref_size.height();
  }
}

gfx::Size SubmenuView::GetPreferredSize() {
  if (GetChildViewCount() == 0)
    return gfx::Size();

  int max_width = 0;
  int height = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size child_pref_size = child->GetPreferredSize();
    max_width = std::max(max_width, child_pref_size.width());
    height += child_pref_size.height();
  }
  gfx::Insets insets = GetInsets();
  return gfx::Size(max_width + insets.width(), height + insets.height());
}

void SubmenuView::DidChangeBounds(const gfx::Rect& previous,
                                  const gfx::Rect& current) {
  SchedulePaint();
}

void SubmenuView::PaintChildren(ChromeCanvas* canvas) {
  View::PaintChildren(canvas);

  if (drop_item_ && drop_position_ != MenuDelegate::DROP_ON)
    PaintDropIndicator(canvas, drop_item_, drop_position_);
}

bool SubmenuView::CanDrop(const OSExchangeData& data) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->CanDrop(this, data);
}

void SubmenuView::OnDragEntered(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuView::OnDragUpdated(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuView::OnDragExited() {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragExited(this);
}

int SubmenuView::OnPerformDrop(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnPerformDrop(this, event);
}

bool SubmenuView::OnMouseWheel(const MouseWheelEvent& e) {
  gfx::Rect vis_bounds = GetVisibleBounds();
  int menu_item_count = GetMenuItemCount();
  if (vis_bounds.height() == height() || !menu_item_count) {
    // All menu items are visible, nothing to scroll.
    return true;
  }

  // Find the index of the first menu item whose y-coordinate is >= visible
  // y-coordinate.
  int first_vis_index = -1;
  for (int i = 0; i < menu_item_count; ++i) {
    MenuItemView* menu_item = GetMenuItemAt(i);
    if (menu_item->y() == vis_bounds.y()) {
      first_vis_index = i;
      break;
    } else if (menu_item->y() > vis_bounds.y()) {
      first_vis_index = std::max(0, i - 1);
      break;
    }
  }
  if (first_vis_index == -1)
    return true;

  // If the first item isn't entirely visible, make it visible, otherwise make
  // the next/previous one entirely visible.
  int delta = abs(e.GetOffset() / WHEEL_DELTA);
  bool scroll_up = (e.GetOffset() > 0);
  while (delta-- > 0) {
    int scroll_amount = 0;
    if (scroll_up) {
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y()) {
        if (first_vis_index != 0) {
          scroll_amount = GetMenuItemAt(first_vis_index - 1)->y() -
                          vis_bounds.y();
          first_vis_index--;
        } else {
          break;
        }
      } else {
        scroll_amount = GetMenuItemAt(first_vis_index)->y() - vis_bounds.y();
      }
    } else {
      if (first_vis_index + 1 == GetMenuItemCount())
        break;
      scroll_amount = GetMenuItemAt(first_vis_index + 1)->y() -
                      vis_bounds.y();
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y())
        first_vis_index++;
    }
    ScrollRectToVisible(0, vis_bounds.y() + scroll_amount, vis_bounds.width(),
                        vis_bounds.height());
    vis_bounds = GetVisibleBounds();
  }

  return true;
}

bool SubmenuView::IsShowing() {
  return host_ && host_->IsVisible();
}

void SubmenuView::ShowAt(HWND parent,
                         const gfx::Rect& bounds,
                         bool do_capture) {
  if (host_) {
    host_->ShowWindow(SW_SHOWNA);
    return;
  }

  host_ = new MenuHost(this);
  // Force construction of the scroll view container.
  GetScrollViewContainer();
  // Make sure the first row is visible.
  ScrollRectToVisible(0, 0, 1, 1);
  host_->Init(parent, bounds, scroll_view_container_, do_capture);
}

void SubmenuView::Close() {
  if (host_) {
    host_->Close();
    host_ = NULL;
  }
}

void SubmenuView::Hide() {
  if (host_)
    host_->HideWindow();
}

void SubmenuView::ReleaseCapture() {
  host_->ReleaseCapture();
}

void SubmenuView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  drop_item_ = item;
  drop_position_ = position;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuView::GetShowSelection(MenuItemView* item) {
  if (drop_item_ == NULL)
    return true;
  // Something is being dropped on one of this menus items. Show the
  // selection if the drop is on the passed in item and the drop position is
  // ON.
  return (drop_item_ == item && drop_position_ == MenuDelegate::DROP_ON);
}

MenuScrollViewContainer* SubmenuView::GetScrollViewContainer() {
  if (!scroll_view_container_) {
    scroll_view_container_ = new MenuScrollViewContainer(this);
    // Otherwise MenuHost would delete us.
    scroll_view_container_->SetParentOwned(false);
  }
  return scroll_view_container_;
}

void SubmenuView::PaintDropIndicator(ChromeCanvas* canvas,
                                     MenuItemView* item,
                                     MenuDelegate::DropPosition position) {
  if (position == MenuDelegate::DROP_NONE)
    return;

  gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
  canvas->FillRectInt(kDropIndicatorColor, bounds.x(), bounds.y(),
                      bounds.width(), bounds.height());
}

void SubmenuView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == NULL)
    return;

  if (position == MenuDelegate::DROP_ON) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DROP_NONE) {
    gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
    SchedulePaint(bounds.x(), bounds.y(), bounds.width(), bounds.height());
  }
}

gfx::Rect SubmenuView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DROP_NONE);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DROP_BEFORE:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DROP_AFTER:
      item_bounds.Offset(0, item_bounds.height() - kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    default:
      // Don't render anything for on.
      return gfx::Rect();
  }
}

// MenuItemView ---------------------------------------------------------------

//  static
const int MenuItemView::kMenuItemViewID = 1001;

// static
bool MenuItemView::allow_task_nesting_during_run_ = false;

MenuItemView::MenuItemView(MenuDelegate* delegate) {
  DCHECK(delegate_);
  Init(NULL, 0, SUBMENU, delegate);
}

MenuItemView::~MenuItemView() {
  if (controller_) {
    // We're currently showing.

    // We can't delete ourselves while we're blocking.
    DCHECK(!controller_->IsBlockingRun());

    // Invoking Cancel is going to call us back and notify the delegate.
    // Notifying the delegate from the destructor can be problematic. To avoid
    // this the delegate is set to NULL.
    delegate_ = NULL;

    controller_->Cancel(true);
  }
  delete submenu_;
}

void MenuItemView::RunMenuAt(HWND parent,
                             const gfx::Rect& bounds,
                             AnchorPosition anchor,
                             bool has_mnemonics) {
  PrepareForRun(has_mnemonics);

  int mouse_event_flags;

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller && !controller->IsBlockingRun()) {
    // A menu is already showing, but it isn't a blocking menu. Cancel it.
    // We can get here during drag and drop if the user right clicks on the
    // menu quickly after the drop.
    controller->Cancel(true);
    controller = NULL;
  }
  bool owns_controller = false;
  if (!controller) {
    // No menus are showing, show one.
    controller = new MenuController(true);
    MenuController::SetActiveInstance(controller);
    owns_controller = true;
  } else {
    // A menu is already showing, use the same controller.

    // Don't support blocking from within non-blocking.
    DCHECK(controller->IsBlockingRun());
  }

  controller_ = controller;

  // Run the loop.
  MenuItemView* result =
      controller->Run(parent, this, bounds, anchor, &mouse_event_flags);

  RemoveEmptyMenus();

  controller_ = NULL;

  if (owns_controller) {
    // We created the controller and need to delete it.
    if (MenuController::GetActiveInstance() == controller)
      MenuController::SetActiveInstance(NULL);
    delete controller;
  }
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  DestroyAllMenuHosts();
  if (result && delegate_)
    delegate_->ExecuteCommand(result->GetCommand(), mouse_event_flags);
}

void MenuItemView::RunMenuForDropAt(HWND parent,
                                    const gfx::Rect& bounds,
                                    AnchorPosition anchor) {
  PrepareForRun(false);

  // If there is a menu, hide it so that only one menu is shown during dnd.
  MenuController* current_controller = MenuController::GetActiveInstance();
  if (current_controller) {
    current_controller->Cancel(true);
  }

  // Always create a new controller for non-blocking.
  controller_ = new MenuController(false);

  // Set the instance, that way it can be canceled by another menu.
  MenuController::SetActiveInstance(controller_);

  controller_->Run(parent, this, bounds, anchor, NULL);
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(true);
  }
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_)
    submenu_ = new SubmenuView(this);
  return submenu_;
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetIcon(const SkBitmap& icon, int item_id) {
  MenuItemView* item = GetDescendantByID(item_id);
  DCHECK(item);
  item->SetIcon(icon);
}

void MenuItemView::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
  SchedulePaint();
}

void MenuItemView::Paint(ChromeCanvas* canvas) {
  Paint(canvas, false);
}

gfx::Size MenuItemView::GetPreferredSize() {
  ChromeFont& font = GetRootMenuItem()->font_;
  return gfx::Size(
      font.GetStringWidth(title_) + label_start + item_right_margin,
      font.height() + GetBottomMargin() + GetTopMargin());
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_;
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  MenuItemView* item = this;
  while (item) {
    MenuItemView* parent = item->GetParentMenuItem();
    if (!parent)
      return item;
    item = parent;
  }
  NOTREACHED();
  return NULL;
}

wchar_t MenuItemView::GetMnemonic() {
  if (!has_mnemonics_)
    return 0;

  const std::wstring& title = GetTitle();
  size_t index = 0;
  do {
    index = title.find('&', index);
    if (index != std::wstring::npos) {
      if (index + 1 != title.size() && title[index + 1] != '&')
        return title[index + 1];
      index++;
    }
  } while (index != std::wstring::npos);
  return 0;
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type) {
  Init(parent, command, type, NULL);
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type,
                        MenuDelegate* delegate) {
  delegate_ = delegate;
  controller_ = NULL;
  canceled_ = false;
  parent_menu_item_ = parent;
  type_ = type;
  selected_ = false;
  command_ = command;
  submenu_ = NULL;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);
  has_icons_ = false;

  MenuDelegate* root_delegate = GetDelegate();
  if (root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

MenuItemView* MenuItemView::AppendMenuItemInternal(int item_id,
                                                   const std::wstring& label,
                                                   const SkBitmap& icon,
                                                   Type type) {
  if (!submenu_)
    CreateSubmenu();
  if (type == SEPARATOR) {
    submenu_->AddChildView(new MenuSeparator());
    return NULL;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetIcon(icon);
  if (type == SUBMENU)
    item->CreateSubmenu();
  submenu_->AddChildView(item);
  return item;
}

MenuItemView* MenuItemView::GetDescendantByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return NULL;
  for (int i = 0; i < GetSubmenu()->GetChildViewCount(); ++i) {
    View* child = GetSubmenu()->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* result = static_cast<MenuItemView*>(child)->
          GetDescendantByID(id);
      if (result)
        return result;
    }
  }
  return NULL;
}

void MenuItemView::DropMenuClosed(bool notify_delegate) {
  DCHECK(controller_);
  DCHECK(!controller_->IsBlockingRun());
  if (MenuController::GetActiveInstance() == controller_)
    MenuController::SetActiveInstance(NULL);
  delete controller_;
  controller_ = NULL;

  RemoveEmptyMenus();

  if (notify_delegate && delegate_) {
    // Our delegate is null when invoked from the destructor.
    delegate_->DropMenuClosed(this);
  }
  // WARNING: its possible the delegate deleted us at this point.
}

void MenuItemView::PrepareForRun(bool has_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Don't invoke run from within run on the same menu.
  DCHECK(!controller_);

  // Force us to have a submenu.
  CreateSubmenu();

  canceled_ = false;

  has_mnemonics_ = has_mnemonics;

  AddEmptyMenus();

  if (!MenuController::GetActiveInstance()) {
    // Only update the menu size if there are no menus showing, otherwise
    // things may shift around.
    UpdateMenuPartSizes(has_icons_);
  }

  font_ = GetMenuFont();

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);
}

int MenuItemView::GetDrawStringFlags() {
  int flags = 0;
  if (UILayoutIsRightToLeft())
    flags |= ChromeCanvas::TEXT_ALIGN_RIGHT;
  else
    flags |= ChromeCanvas::TEXT_ALIGN_LEFT;

  if (has_mnemonics_) {
    if (show_mnemonics)
      flags |= ChromeCanvas::SHOW_PREFIX;
    else
      flags |= ChromeCanvas::HIDE_PREFIX;
  }
  return flags;
}

void MenuItemView::AddEmptyMenus() {
  DCHECK(HasSubmenu());
  if (submenu_->GetChildViewCount() == 0) {
    submenu_->AddChildView(0, new EmptyMenuMenuItem(this));
  } else {
    for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
         ++i) {
      MenuItemView* child = submenu_->GetMenuItemAt(i);
      if (child->HasSubmenu())
        child->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Iterate backwards as we may end up removing views, which alters the child
  // view count.
  for (int i = submenu_->GetChildViewCount() - 1; i >= 0; --i) {
    View* child = submenu_->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(RECT* rect) const {
  gfx::Rect mirrored_rect(*rect);
  mirrored_rect.set_x(MirroredLeftPointForRect(mirrored_rect));
  *rect = mirrored_rect.ToRECT();
}

void MenuItemView::Paint(ChromeCanvas* canvas, bool for_drag) {
  bool render_selection =
      (!for_drag && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this));
  int state = render_selection ? MPI_HOT :
                                 (IsEnabled() ? MPI_NORMAL : MPI_DISABLED);
  HDC dc = canvas->beginPlatformPaint();

  // The gutter is rendered before the background.
  if (render_gutter && !for_drag) {
    RECT gutter_bounds = { label_start - kGutterToLabel - gutter_width, 0, 0,
                           height() };
    gutter_bounds.right = gutter_bounds.left + gutter_width;
    AdjustBoundsForRTLUI(&gutter_bounds);
    NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL, &gutter_bounds);
  }

  // Render the background.
  if (!for_drag) {
    RECT item_bounds = { 0, 0, width(), height() };
    AdjustBoundsForRTLUI(&item_bounds);
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, state, render_selection,
        &item_bounds);
  }

  int icon_x = kItemLeftMargin;
  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();
  int icon_y = top_margin + (height() - kItemTopMargin -
                             bottom_margin - check_height) / 2;
  int icon_height = check_height;
  int icon_width = check_width;

  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    // Draw the check background.
    RECT check_bg_bounds = { 0, 0, icon_x + icon_width, height() };
    const int bg_state = IsEnabled() ? MCB_NORMAL : MCB_DISABLED;
    AdjustBoundsForRTLUI(&check_bg_bounds);
    NativeTheme::instance()->PaintMenuCheckBackground(
        NativeTheme::MENU, dc, MENU_POPUPCHECKBACKGROUND, bg_state,
        &check_bg_bounds);

    // And the check.
    RECT check_bounds = { icon_x, icon_y, icon_x + icon_width,
                          icon_y + icon_height };
    const int check_state = IsEnabled() ? MC_CHECKMARKNORMAL :
                                          MC_CHECKMARKDISABLED;
    AdjustBoundsForRTLUI(&check_bounds);
    NativeTheme::instance()->PaintMenuCheck(
        NativeTheme::MENU, dc, MENU_POPUPCHECK, check_state, &check_bounds,
        render_selection);
  }

  // Render the foreground.
  // Menu color is specific to Vista, fallback to classic colors if can't
  // get color.
  int default_sys_color = render_selection ? COLOR_HIGHLIGHTTEXT :
      (IsEnabled() ? COLOR_MENUTEXT : COLOR_GRAYTEXT);
  SkColor fg_color = NativeTheme::instance()->GetThemeColorWithDefault(
      NativeTheme::MENU, MENU_POPUPITEM, state, TMT_TEXTCOLOR, default_sys_color);
  int width = this->width() - item_right_margin - label_start;
  ChromeFont& font = GetRootMenuItem()->font_;
  gfx::Rect text_bounds(label_start, top_margin, width, font.height());
  text_bounds.set_x(MirroredLeftPointForRect(text_bounds));
  canvas->DrawStringInt(GetTitle(), font, fg_color,
                        text_bounds.x(), text_bounds.y(), text_bounds.width(),
                        text_bounds.height(),
                        GetRootMenuItem()->GetDrawStringFlags());

  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(kItemLeftMargin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }

  if (HasSubmenu()) {
    int state_id = IsEnabled() ? MSM_NORMAL : MSM_DISABLED;
    RECT arrow_bounds = { this->width() - item_right_margin + kLabelToArrowPadding,
                          0, 0, height() };
    arrow_bounds.right = arrow_bounds.left + arrow_width;
    AdjustBoundsForRTLUI(&arrow_bounds);

    // If our sub menus open from right to left (which is the case when the
    // locale is RTL) then we should make sure the menu arrow points to the
    // right direction.
    NativeTheme::MenuArrowDirection arrow_direction;
    if (UILayoutIsRightToLeft())
      arrow_direction = NativeTheme::LEFT_POINTING_ARROW;
    else
      arrow_direction = NativeTheme::RIGHT_POINTING_ARROW;

    NativeTheme::instance()->PaintMenuArrow(
        NativeTheme::MENU, dc, MENU_POPUPSUBMENU, state_id, &arrow_bounds,
        arrow_direction, render_selection);
  }
  canvas->endPlatformPaint();
}

void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
       ++i) {
    submenu_->GetMenuItemAt(i)->DestroyAllMenuHosts();
  }
}

int MenuItemView::GetTopMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_ ? kItemTopMargin : kItemNoIconTopMargin;
}

int MenuItemView::GetBottomMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_ ?
      kItemBottomMargin : kItemNoIconBottomMargin;
}

// MenuController ------------------------------------------------------------

// static
MenuController* MenuController::active_instance_ = NULL;

// static
MenuController* MenuController::GetActiveInstance() {
  return active_instance_;
}

#ifdef DEBUG_MENU
static int instance_count = 0;
static int nested_depth = 0;
#endif

MenuItemView* MenuController::Run(HWND parent,
                                  MenuItemView* root,
                                  const gfx::Rect& bounds,
                                  MenuItemView::AnchorPosition position,
                                  int* result_mouse_event_flags) {
  exit_all_ = false;
  possible_drag_ = false;

  bool nested_menu = showing_;
  if (showing_) {
    // Only support nesting of blocking_run menus, nesting of
    // blocking/non-blocking shouldn't be needed.
    DCHECK(blocking_run_);

    // We're already showing, push the current state.
    menu_stack_.push_back(state_);

    // The context menu should be owned by the same parent.
    DCHECK(owner_ == parent);
  } else {
    showing_ = true;
  }

  // Reset current state.
  pending_state_ = State();
  state_ = State();
  pending_state_.initial_bounds = bounds;
  if (bounds.height() > 1) {
    // Inset the bounds slightly, otherwise drag coordinates don't line up
    // nicely and menus close prematurely.
    pending_state_.initial_bounds.Inset(0, 1);
  }
  pending_state_.anchor = position;
  owner_ = parent;

  // Calculate the bounds of the monitor we'll show menus on. Do this once to
  // avoid repeated system queries for the info.
  POINT initial_loc = { bounds.x(), bounds.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  if (monitor) {
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    // Menus appear over the taskbar.
    pending_state_.monitor_bounds = gfx::Rect(mi.rcMonitor);
  }

  any_menu_contains_mouse_ = false;

  // Set the selection, which opens the initial menu.
  SetSelection(root, true, true);

  if (!blocking_run_) {
    // Start the timer to hide the menu. This is needed as we get no
    // notification when the drag has finished.
    StartCancelAllTimer();
    return NULL;
  }

#ifdef DEBUG_MENU
  nested_depth++;
  DLOG(INFO) << " entering nested loop, depth=" << nested_depth;
#endif

  MessageLoopForUI* loop = MessageLoopForUI::current();
  if (MenuItemView::allow_task_nesting_during_run_) {
    bool did_allow_task_nesting = loop->NestableTasksAllowed();
    loop->SetNestableTasksAllowed(true);
    loop->Run(this);
    loop->SetNestableTasksAllowed(did_allow_task_nesting);
  } else {
    loop->Run(this);
  }

#ifdef DEBUG_MENU
  nested_depth--;
  DLOG(INFO) << " exiting nested loop,  depth=" << nested_depth;
#endif

  // Close any open menus.
  SetSelection(NULL, false, true);

  if (nested_menu) {
    DCHECK(!menu_stack_.empty());
    // We're running from within a menu, restore the previous state.
    // The menus are already showing, so we don't have to show them.
    state_ = menu_stack_.back();
    pending_state_ = menu_stack_.back();
    menu_stack_.pop_back();
  } else {
    showing_ = false;
    did_capture_ = false;
  }

  MenuItemView* result = result_;
  // In case we're nested, reset result_.
  result_ = NULL;

  if (result_mouse_event_flags)
    *result_mouse_event_flags = result_mouse_event_flags_;

  if (nested_menu && result) {
    // We're nested and about to return a value. The caller might enter another
    // blocking loop. We need to make sure all menus are hidden before that
    // happens otherwise the menus will stay on screen.
    CloseAllNestedMenus();

    // Set exit_all_ to true, which makes sure all nested loops exit
    // immediately.
    exit_all_ = true;
  }

  return result;
}

void MenuController::SetSelection(MenuItemView* menu_item,
                                  bool open_submenu,
                                  bool update_immediately) {
  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(pending_state_.item, menu_item, &current_path,
                             &new_path, &paths_differ_at);

  size_t current_size = current_path.size();
  size_t new_size = new_path.size();

  // Notify the old path it isn't selected.
  for (size_t i = paths_differ_at; i < current_size; ++i)
    current_path[i]->SetSelected(false);

  // Notify the new path it is selected.
  for (size_t i = paths_differ_at; i < new_size; ++i)
    new_path[i]->SetSelected(true);

  if (menu_item && menu_item->GetDelegate())
    menu_item->GetDelegate()->SelectionChanged(menu_item);

  pending_state_.item = menu_item;
  pending_state_.submenu_open = open_submenu;

  // Stop timers.
  StopShowTimer();
  StopCancelAllTimer();

  if (update_immediately)
    CommitPendingSelection();
  else
    StartShowTimer();
}

void MenuController::Cancel(bool all) {
  if (!showing_) {
    // This occurs if we're in the process of notifying the delegate for a drop
    // and the delegate cancels us.
    return;
  }

  MenuItemView* selected = state_.item;
  exit_all_ = all;

  // Hide windows immediately.
  SetSelection(NULL, false, true);

  if (!blocking_run_) {
    // If we didn't block the caller we need to notify the menu, which
    // triggers deleting us.
    DCHECK(selected);
    showing_ = false;
    selected->GetRootMenuItem()->DropMenuClosed(true);
    // WARNING: the call to MenuClosed deletes us.
    return;
  }
}

void MenuController::OnMousePressed(SubmenuView* source,
                                    const MouseEvent& event) {
#ifdef DEBUG_MENU
  DLOG(INFO) << "OnMousePressed source=" << source;
#endif
  if (!blocking_run_)
    return;

  MenuPart part =
      GetMenuPartByScreenCoordinate(source, event.x(), event.y());
  if (part.is_scroll())
    return;  // Ignore presses on scroll buttons.

  if (part.type == MenuPart::NONE ||
      (part.type == MenuPart::MENU_ITEM && part.menu &&
       part.menu->GetRootMenuItem() != state_.item->GetRootMenuItem())) {
    // Mouse wasn't pressed over any menu, or the active menu, cancel.

    // We're going to close and we own the mouse capture. We need to repost the
    // mouse down, otherwise the window the user clicked on won't get the
    // event.
    RepostEvent(source, event);

    // And close.
    Cancel(true);
    return;
  }

  any_menu_contains_mouse_ = true;

  bool open_submenu = false;
  if (!part.menu) {
    part.menu = source->GetMenuItem();
    open_submenu = true;
  } else {
    if (part.menu->GetDelegate()->CanDrag(part.menu)) {
      possible_drag_ = true;
      press_x_ = event.x();
      press_y_ = event.y();
    }
    if (part.menu->HasSubmenu())
      open_submenu = true;
  }
  // On a press we immediately commit the selection, that way a submenu
  // pops up immediately rather than after a delay.
  SetSelection(part.menu, open_submenu, true);
}

void MenuController::OnMouseDragged(SubmenuView* source,
                                    const MouseEvent& event) {
#ifdef DEBUG_MENU
  DLOG(INFO) << "OnMouseDragged source=" << source;
#endif
  MenuPart part =
      GetMenuPartByScreenCoordinate(source, event.x(), event.y());
  UpdateScrolling(part);

  if (!blocking_run_)
    return;

  if (possible_drag_) {
    if (View::ExceededDragThreshold(event.x() - press_x_,
                                    event.y() - press_y_)) {
      MenuItemView* item = state_.item;
      DCHECK(item);
      // Points are in the coordinates of the submenu, need to map to that of
      // the selected item. Additionally source may not be the parent of
      // the selected item, so need to map to screen first then to item.
      gfx::Point press_loc(press_x_, press_y_);
      View::ConvertPointToScreen(source->GetScrollViewContainer(), &press_loc);
      View::ConvertPointToView(NULL, item, &press_loc);
      gfx::Point drag_loc(event.location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(), &drag_loc);
      View::ConvertPointToView(NULL, item, &drag_loc);
      ChromeCanvas canvas(item->width(), item->height(), false);
      item->Paint(&canvas, true);

      scoped_refptr<OSExchangeData> data(new OSExchangeData);
      item->GetDelegate()->WriteDragData(item, data.get());
      drag_utils::SetDragImageOnDataObject(canvas, item->width(),
                                           item->height(), press_loc.x(),
                                           press_loc.y(), data);

      scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
      int drag_ops = item->GetDelegate()->GetDragOperations(item);
      DWORD effects;
      StopScrolling();
      DoDragDrop(data, drag_source,
                 DragDropTypes::DragOperationToDropEffect(drag_ops),
                 &effects);
      if (GetActiveInstance() == this) {
        if (showing_ ) {
          // We're still showing, close all menus.
          CloseAllNestedMenus();
          Cancel(true);
        } // else case, drop was on us.
      } // else case, someone canceled us, don't do anything
    }
    return;
  }
  if (part.type == MenuPart::MENU_ITEM) {
    if (!part.menu)
      part.menu = source->GetMenuItem();
    SetSelection(part.menu ? part.menu : state_.item, true, false);
  }
  any_menu_contains_mouse_ = (part.type == MenuPart::MENU_ITEM);
}

void MenuController::OnMouseReleased(SubmenuView* source,
                                     const MouseEvent& event) {
#ifdef DEBUG_MENU
  DLOG(INFO) << "OnMouseReleased source=" << source;
#endif
  if (!blocking_run_)
    return;

  DCHECK(state_.item);
  possible_drag_ = false;
  DCHECK(blocking_run_);
  MenuPart part =
      GetMenuPartByScreenCoordinate(source, event.x(), event.y());
  any_menu_contains_mouse_ = (part.type == MenuPart::MENU_ITEM);
  if (event.IsRightMouseButton() && (part.type == MenuPart::MENU_ITEM &&
                                     part.menu)) {
    // Set the selection immediately, making sure the submenu is only open
    // if it already was.
    bool open_submenu = (state_.item == pending_state_.item &&
                         state_.submenu_open);
    SetSelection(pending_state_.item, open_submenu, true);
    gfx::Point loc(event.location());
    View::ConvertPointToScreen(source->GetScrollViewContainer(), &loc);

    // If we open a context menu just return now
    if (part.menu->GetDelegate()->ShowContextMenu(
        part.menu, part.menu->GetCommand(), loc.x(), loc.y(), true))
      return;
  }

  if (!part.is_scroll() && part.menu && !part.menu->HasSubmenu()) {
    if (part.menu->GetDelegate()->IsTriggerableEvent(event)) {
      Accept(part.menu, event.GetFlags());
      return;
    }
  } else if (part.type == MenuPart::MENU_ITEM) {
    // User either clicked on empty space, or a menu that has children.
    SetSelection(part.menu ? part.menu : state_.item, true, true);
  }
}

void MenuController::OnMouseMoved(SubmenuView* source,
                                  const MouseEvent& event) {
#ifdef DEBUG_MENU
  DLOG(INFO) << "OnMouseMoved source=" << source;
#endif
  if (showing_submenu_)
    return;

  MenuPart part =
      GetMenuPartByScreenCoordinate(source, event.x(), event.y());

  UpdateScrolling(part);

  if (!blocking_run_)
    return;

  any_menu_contains_mouse_ = (part.type == MenuPart::MENU_ITEM);
  if (part.type == MenuPart::MENU_ITEM && part.menu) {
    SetSelection(part.menu, true, false);
  } else if (!part.is_scroll() && any_menu_contains_mouse_ &&
             pending_state_.item &&
             (!pending_state_.item->HasSubmenu() ||
              !pending_state_.item->GetSubmenu()->IsShowing())) {
    // On exit if the user hasn't selected an item with a submenu, move the
    // selection back to the parent menu item.
    SetSelection(pending_state_.item->GetParentMenuItem(), true, false);
    any_menu_contains_mouse_ = false;
  }
}

void MenuController::OnMouseEntered(SubmenuView* source,
                                    const MouseEvent& event) {
  // MouseEntered is always followed by a mouse moved, so don't need to
  // do anything here.
}

bool MenuController::CanDrop(SubmenuView* source, const OSExchangeData& data) {
  return source->GetMenuItem()->GetDelegate()->CanDrop(source->GetMenuItem(),
                                                       data);
}

void MenuController::OnDragEntered(SubmenuView* source,
                                   const DropTargetEvent& event) {
  valid_drop_coordinates_ = false;
}

int MenuController::OnDragUpdated(SubmenuView* source,
                                  const DropTargetEvent& event) {
  StopCancelAllTimer();

  gfx::Point screen_loc(event.location());
  View::ConvertPointToScreen(source, &screen_loc);
  if (valid_drop_coordinates_ && screen_loc.x() == drop_x_ &&
      screen_loc.y() == drop_y_) {
    return last_drop_operation_;
  }
  drop_x_ = screen_loc.x();
  drop_y_ = screen_loc.y();
  valid_drop_coordinates_ = true;

  MenuItemView* menu_item = GetMenuItemAt(source, event.x(), event.y());
  bool over_empty_menu = false;
  if (!menu_item) {
    // See if we're over an empty menu.
    menu_item = GetEmptyMenuItemAt(source, event.x(), event.y());
    if (menu_item)
      over_empty_menu = true;
  }
  MenuDelegate::DropPosition drop_position = MenuDelegate::DROP_NONE;
  int drop_operation = DragDropTypes::DRAG_NONE;
  if (menu_item) {
    gfx::Point menu_item_loc(event.location());
    View::ConvertPointToView(source, menu_item, &menu_item_loc);
    MenuItemView* query_menu_item;
    if (!over_empty_menu) {
      int menu_item_height = menu_item->height();
      if (menu_item->HasSubmenu() &&
          (menu_item_loc.y() > kDropBetweenPixels &&
           menu_item_loc.y() < (menu_item_height - kDropBetweenPixels))) {
        drop_position = MenuDelegate::DROP_ON;
      } else if (menu_item_loc.y() < menu_item_height / 2) {
        drop_position = MenuDelegate::DROP_BEFORE;
      } else {
        drop_position = MenuDelegate::DROP_AFTER;
      }
      query_menu_item = menu_item;
    } else {
      query_menu_item = menu_item->GetParentMenuItem();
      drop_position = MenuDelegate::DROP_ON;
    }
    drop_operation = menu_item->GetDelegate()->GetDropOperation(
        query_menu_item, event, &drop_position);

    if (menu_item->HasSubmenu()) {
      // The menu has a submenu, schedule the submenu to open.
      SetSelection(menu_item, true, false);
    } else {
      SetSelection(menu_item, false, false);
    }

    if (drop_position == MenuDelegate::DROP_NONE ||
        drop_operation == DragDropTypes::DRAG_NONE) {
      menu_item = NULL;
    }
  } else {
    SetSelection(source->GetMenuItem(), true, false);
  }
  SetDropMenuItem(menu_item, drop_position);
  last_drop_operation_ = drop_operation;
  return drop_operation;
}

void MenuController::OnDragExited(SubmenuView* source) {
  StartCancelAllTimer();

  if (drop_target_) {
    StopShowTimer();
    SetDropMenuItem(NULL, MenuDelegate::DROP_NONE);
  }
}

int MenuController::OnPerformDrop(SubmenuView* source,
                                  const DropTargetEvent& event) {
  DCHECK(drop_target_);
  // NOTE: the delegate may delete us after invoking OnPerformDrop, as such
  // we don't call cancel here.

  MenuItemView* item = state_.item;
  DCHECK(item);

  MenuItemView* drop_target = drop_target_;
  MenuDelegate::DropPosition drop_position = drop_position_;

  // Close all menus, including any nested menus.
  SetSelection(NULL, false, true);
  CloseAllNestedMenus();

  // Set state such that we exit.
  showing_ = false;
  exit_all_ = true;

  if (!IsBlockingRun())
    item->GetRootMenuItem()->DropMenuClosed(false);

  // WARNING: the call to MenuClosed deletes us.

  // If over an empty menu item, drop occurs on the parent.
  if (drop_target->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID)
    drop_target = drop_target->GetParentMenuItem();

  return drop_target->GetDelegate()->OnPerformDrop(
      drop_target, drop_position, event);
}

void MenuController::OnDragEnteredScrollButton(SubmenuView* source,
                                               bool is_up) {
  MenuPart part;
  part.type = is_up ? MenuPart::SCROLL_UP : MenuPart::SCROLL_DOWN;
  part.submenu = source;
  UpdateScrolling(part);

  // Do this to force the selection to hide.
  SetDropMenuItem(source->GetMenuItemAt(0), MenuDelegate::DROP_NONE);

  StopCancelAllTimer();
}

void MenuController::OnDragExitedScrollButton(SubmenuView* source) {
  StartCancelAllTimer();
  SetDropMenuItem(NULL, MenuDelegate::DROP_NONE);
  StopScrolling();
}

// static
void MenuController::SetActiveInstance(MenuController* controller) {
  active_instance_ = controller;
}

bool MenuController::Dispatch(const MSG& msg) {
  DCHECK(blocking_run_);

  if (exit_all_) {
    // We must translate/dispatch the message here, otherwise we would drop
    // the message on the floor.
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    return false;
  }

  // NOTE: we don't get WM_ACTIVATE or anything else interesting in here.
  switch (msg.message) {
    case WM_CONTEXTMENU: {
      MenuItemView* item = pending_state_.item;
      if (item && item->GetRootMenuItem() != item) {
        gfx::Point screen_loc(0, item->height());
        View::ConvertPointToScreen(item, &screen_loc);
        item->GetDelegate()->ShowContextMenu(
            item, item->GetCommand(), screen_loc.x(), screen_loc.y(), false);
      }
      return true;
    }

    // NOTE: focus wasn't changed when the menu was shown. As such, don't
    // dispatch key events otherwise the focused window will get the events.
    case WM_KEYDOWN:
      return OnKeyDown(msg);

    case WM_CHAR:
      return OnChar(msg);

    case WM_KEYUP:
      return true;

    case WM_SYSKEYUP:
      // We may have been shown on a system key, as such don't do anything
      // here. If another system key is pushed we'll get a WM_SYSKEYDOWN and
      // close the menu.
      return true;

    case WM_CANCELMODE:
    case WM_SYSKEYDOWN:
      // Exit immediately on system keys.
      Cancel(true);
      return false;

    default:
      break;
  }
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return !exit_all_;
}

bool MenuController::OnKeyDown(const MSG& msg) {
  DCHECK(blocking_run_);

  switch (msg.wParam) {
    case VK_UP:
      IncrementSelection(-1);
      break;

    case VK_DOWN:
      IncrementSelection(1);
      break;

    // Handling of VK_RIGHT and VK_LEFT is different depending on the UI
    // layout.
    case VK_RIGHT:
      if (l10n_util::TextDirection() == l10n_util::RIGHT_TO_LEFT)
        CloseSubmenu();
      else
        OpenSubmenuChangeSelectionIfCan();
      break;

    case VK_LEFT:
      if (l10n_util::TextDirection() == l10n_util::RIGHT_TO_LEFT)
        OpenSubmenuChangeSelectionIfCan();
      else
        CloseSubmenu();
      break;

    case VK_RETURN:
      if (pending_state_.item) {
        if (pending_state_.item->HasSubmenu()) {
          OpenSubmenuChangeSelectionIfCan();
        } else if (pending_state_.item->IsEnabled()) {
          Accept(pending_state_.item, 0);
          return false;
        }
      }
      break;

    case VK_ESCAPE:
      if (!state_.item->GetParentMenuItem() ||
          (!state_.item->GetParentMenuItem()->GetParentMenuItem() &&
           (!state_.item->HasSubmenu() ||
            !state_.item->GetSubmenu()->IsShowing()))) {
        // User pressed escape and only one menu is shown, cancel it.
        Cancel(false);
        return false;
      } else {
        CloseSubmenu();
      }
      break;

    case VK_APPS:
      break;

    default:
      TranslateMessage(&msg);
      break;
  }
  return true;
}

bool MenuController::OnChar(const MSG& msg) {
  DCHECK(blocking_run_);

  return !SelectByChar(static_cast<wchar_t>(msg.wParam));
}

MenuController::MenuController(bool blocking)
    : blocking_run_(blocking),
      showing_(false),
      exit_all_(false),
      did_capture_(false),
      result_(NULL),
      drop_target_(NULL),
      owner_(NULL),
      possible_drag_(false),
      valid_drop_coordinates_(false),
      any_menu_contains_mouse_(false),
      showing_submenu_(false),
      result_mouse_event_flags_(0) {
#ifdef DEBUG_MENU
  instance_count++;
  DLOG(INFO) << "created MC, count=" << instance_count;
#endif
}

MenuController::~MenuController() {
  DCHECK(!showing_);
  StopShowTimer();
  StopCancelAllTimer();
#ifdef DEBUG_MENU
  instance_count--;
  DLOG(INFO) << "destroyed MC, count=" << instance_count;
#endif
}

void MenuController::Accept(MenuItemView* item, int mouse_event_flags) {
  DCHECK(IsBlockingRun());
  result_ = item;
  exit_all_ = true;
  result_mouse_event_flags_ = mouse_event_flags;
}

void MenuController::CloseAllNestedMenus() {
  for (std::list<State>::iterator i = menu_stack_.begin();
       i != menu_stack_.end(); ++i) {
    MenuItemView* item = i->item;
    MenuItemView* last_item = item;
    while (item) {
      CloseMenu(item);
      last_item = item;
      item = item->GetParentMenuItem();
    }
    i->submenu_open = false;
    i->item = last_item;
  }
}

MenuItemView* MenuController::GetMenuItemAt(View* source, int x, int y) {
  View* child_under_mouse = source->GetViewForPoint(gfx::Point(x, y));
  if (child_under_mouse && child_under_mouse->IsEnabled() &&
      child_under_mouse->GetID() == MenuItemView::kMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return NULL;
}

MenuItemView* MenuController::GetEmptyMenuItemAt(View* source, int x, int y) {
  View* child_under_mouse = source->GetViewForPoint(gfx::Point(x, y));
  if (child_under_mouse &&
      child_under_mouse->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return NULL;
}

bool MenuController::IsScrollButtonAt(SubmenuView* source,
                                      int x,
                                      int y,
                                      MenuPart::Type* part) {
  MenuScrollViewContainer* scroll_view = source->GetScrollViewContainer();
  View* child_under_mouse = scroll_view->GetViewForPoint(gfx::Point(x, y));
  if (child_under_mouse && child_under_mouse->IsEnabled()) {
    if (child_under_mouse == scroll_view->scroll_up_button()) {
      *part = MenuPart::SCROLL_UP;
      return true;
    }
    if (child_under_mouse == scroll_view->scroll_down_button()) {
      *part = MenuPart::SCROLL_DOWN;
      return true;
    }
  }
  return false;
}

MenuController::MenuPart MenuController::GetMenuPartByScreenCoordinate(
    SubmenuView* source,
    int source_x,
    int source_y) {
  MenuPart part;

  gfx::Point screen_loc(source_x, source_y);
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);

  MenuItemView* item = state_.item;
  while (item) {
    if (item->HasSubmenu() && item->GetSubmenu()->IsShowing() &&
        GetMenuPartByScreenCoordinateImpl(item->GetSubmenu(), screen_loc,
                                          &part)) {
      return part;
    }
    item = item->GetParentMenuItem();
  }

  return part;
}

bool MenuController::GetMenuPartByScreenCoordinateImpl(
    SubmenuView* menu,
    const gfx::Point& screen_loc,
    MenuPart* part) {
  // Is the mouse over the scroll buttons?
  gfx::Point scroll_view_loc = screen_loc;
  View* scroll_view_container = menu->GetScrollViewContainer();
  View::ConvertPointToView(NULL, scroll_view_container, &scroll_view_loc);
  if (scroll_view_loc.x() < 0 ||
      scroll_view_loc.x() >= scroll_view_container->width() ||
      scroll_view_loc.y() < 0 ||
      scroll_view_loc.y() >= scroll_view_container->height()) {
    // Point isn't contained in menu.
    return false;
  }
  if (IsScrollButtonAt(menu, scroll_view_loc.x(), scroll_view_loc.y(),
                       &(part->type))) {
    part->submenu = menu;
    return true;
  }

  // Not over the scroll button. Check the actual menu.
  if (DoesSubmenuContainLocation(menu, screen_loc)) {
    gfx::Point menu_loc = screen_loc;
    View::ConvertPointToView(NULL, menu, &menu_loc);
    part->menu = GetMenuItemAt(menu, menu_loc.x(), menu_loc.y());
    part->type = MenuPart::MENU_ITEM;
    return true;
  }

  // While the mouse isn't over a menu item or the scroll buttons of menu, it
  // is contained by menu and so we return true. If we didn't return true other
  // menus would be searched, even though they are likely obscured by us.
  return true;
}

bool MenuController::DoesSubmenuContainLocation(SubmenuView* submenu,
                                                const gfx::Point& screen_loc) {
  gfx::Point view_loc = screen_loc;
  View::ConvertPointToView(NULL, submenu, &view_loc);
  gfx::Rect vis_rect = submenu->GetVisibleBounds();
  return vis_rect.Contains(view_loc.x(), view_loc.y());
}

void MenuController::CommitPendingSelection() {
  StopShowTimer();

  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(state_.item, pending_state_.item, &current_path,
                             &new_path, &paths_differ_at);

  // Hide the old menu.
  for (size_t i = paths_differ_at; i < current_path.size(); ++i) {
    if (current_path[i]->HasSubmenu()) {
      current_path[i]->GetSubmenu()->Hide();
    }
  }

  // Copy pending to state_, making sure to preserve the direction menus were
  // opened.
  std::list<bool> pending_open_direction;
  state_.open_leading.swap(pending_open_direction);
  state_ = pending_state_;
  state_.open_leading.swap(pending_open_direction);

  int menu_depth = MenuDepth(state_.item);
  if (menu_depth == 0) {
    state_.open_leading.clear();
  } else {
    int cached_size = static_cast<int>(state_.open_leading.size());
    DCHECK(menu_depth >= 0);
    while (cached_size-- >= menu_depth)
      state_.open_leading.pop_back();
  }

  if (!state_.item) {
    // Nothing to select.
    StopScrolling();
    return;
  }

  // Open all the submenus preceeding the last menu item (last menu item is
  // handled next).
  if (new_path.size() > 1) {
    for (std::vector<MenuItemView*>::iterator i = new_path.begin();
         i != new_path.end() - 1; ++i) {
      OpenMenu(*i);
    }
  }

  if (state_.submenu_open) {
    // The submenu should be open, open the submenu if the item has a submenu.
    if (state_.item->HasSubmenu()) {
      OpenMenu(state_.item);
    } else {
      state_.submenu_open = false;
    }
  } else if (state_.item->HasSubmenu() &&
             state_.item->GetSubmenu()->IsShowing()) {
    state_.item->GetSubmenu()->Hide();
  }

  if (scroll_task_.get() && scroll_task_->submenu()) {
    // Stop the scrolling if none of the elements of the selection contain
    // the menu being scrolled.
    bool found = false;
    MenuItemView* item = state_.item;
    while (item && !found) {
      found = (item->HasSubmenu() && item->GetSubmenu()->IsShowing() &&
               item->GetSubmenu() == scroll_task_->submenu());
      item = item->GetParentMenuItem();
    }
    if (!found)
      StopScrolling();
  }
}

void MenuController::CloseMenu(MenuItemView* item) {
  DCHECK(item);
  if (!item->HasSubmenu())
    return;
  item->GetSubmenu()->Hide();
}

void MenuController::OpenMenu(MenuItemView* item) {
  DCHECK(item);
  if (item->GetSubmenu()->IsShowing()) {
    return;
  }

  bool prefer_leading =
      state_.open_leading.empty() ? true : state_.open_leading.back();
  bool resulting_direction;
  gfx::Rect bounds =
      CalculateMenuBounds(item, prefer_leading, &resulting_direction);
  state_.open_leading.push_back(resulting_direction);
  bool do_capture = (!did_capture_ && blocking_run_);
  showing_submenu_ = true;
  item->GetSubmenu()->ShowAt(owner_, bounds, do_capture);
  showing_submenu_ = false;
  did_capture_ = true;
}

void MenuController::BuildPathsAndCalculateDiff(
    MenuItemView* old_item,
    MenuItemView* new_item,
    std::vector<MenuItemView*>* old_path,
    std::vector<MenuItemView*>* new_path,
    size_t* first_diff_at) {
  DCHECK(old_path && new_path && first_diff_at);
  BuildMenuItemPath(old_item, old_path);
  BuildMenuItemPath(new_item, new_path);

  size_t common_size = std::min(old_path->size(), new_path->size());

  // Find the first difference between the two paths, when the loop
  // returns, diff_i is the first index where the two paths differ.
  for (size_t i = 0; i < common_size; ++i) {
    if ((*old_path)[i] != (*new_path)[i]) {
      *first_diff_at = i;
      return;
    }
  }

  *first_diff_at = common_size;
}

void MenuController::BuildMenuItemPath(MenuItemView* item,
                                       std::vector<MenuItemView*>* path) {
  if (!item)
    return;
  BuildMenuItemPath(item->GetParentMenuItem(), path);
  path->push_back(item);
}

void MenuController::StartShowTimer() {
  show_timer_.Start(TimeDelta::FromMilliseconds(kShowDelay), this,
                    &MenuController::CommitPendingSelection);
}

void MenuController::StopShowTimer() {
  show_timer_.Stop();
}

void MenuController::StartCancelAllTimer() {
  cancel_all_timer_.Start(TimeDelta::FromMilliseconds(kCloseOnExitTime),
                          this, &MenuController::CancelAll);
}

void MenuController::StopCancelAllTimer() {
  cancel_all_timer_.Stop();
}

gfx::Rect MenuController::CalculateMenuBounds(MenuItemView* item,
                                              bool prefer_leading,
                                              bool* is_leading) {
  DCHECK(item);

  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);

  gfx::Size pref = submenu->GetScrollViewContainer()->GetPreferredSize();

  // Don't let the menu go to wide. This is some where between what IE and FF
  // do.
  pref.set_width(std::min(pref.width(), kMaxMenuWidth));
  if (!state_.monitor_bounds.IsEmpty())
    pref.set_width(std::min(pref.width(), state_.monitor_bounds.width()));

  // Assume we can honor prefer_leading.
  *is_leading = prefer_leading;

  int x, y;

  if (!item->GetParentMenuItem()) {
    // First item, position relative to initial location.
    x = state_.initial_bounds.x();
    y = state_.initial_bounds.bottom();
    if (state_.anchor == MenuItemView::TOPRIGHT)
      x = x + state_.initial_bounds.width() - pref.width();
    if (!state_.monitor_bounds.IsEmpty() &&
        y + pref.height() > state_.monitor_bounds.bottom()) {
      // The menu doesn't fit on screen. If the first location is above the
      // half way point, show from the mouse location to bottom of screen.
      // Otherwise show from the top of the screen to the location of the mouse.
      // While odd, this behavior matches IE.
      if (y < (state_.monitor_bounds.y() +
               state_.monitor_bounds.height() / 2)) {
        pref.set_height(std::min(pref.height(),
                                 state_.monitor_bounds.bottom() - y));
      } else {
        pref.set_height(std::min(pref.height(),
            state_.initial_bounds.y() - state_.monitor_bounds.y()));
        y = state_.initial_bounds.y() - pref.height();
      }
    }
  } else {
    // Not the first menu; position it relative to the bounds of the menu
    // item.
    gfx::Point item_loc;
    View::ConvertPointToScreen(item, &item_loc);

    // We must make sure we take into account the UI layout. If the layout is
    // RTL, then a 'leading' menu is positioned to the left of the parent menu
    // item and not to the right.
    bool layout_is_rtl = item->UILayoutIsRightToLeft();
    bool create_on_the_right = (prefer_leading && !layout_is_rtl) ||
                               (!prefer_leading && layout_is_rtl);

    if (create_on_the_right) {
      x = item_loc.x() + item->width() - kSubmenuHorizontalInset;
      if (state_.monitor_bounds.width() != 0 &&
          x + pref.width() > state_.monitor_bounds.right()) {
        if (layout_is_rtl)
          *is_leading = true;
        else
          *is_leading = false;
        x = item_loc.x() - pref.width() + kSubmenuHorizontalInset;
      }
    } else {
      x = item_loc.x() - pref.width() + kSubmenuHorizontalInset;
      if (state_.monitor_bounds.width() != 0 && x < state_.monitor_bounds.x()) {
        if (layout_is_rtl)
          *is_leading = false;
        else
          *is_leading = true;
        x = item_loc.x() + item->width() - kSubmenuHorizontalInset;
      }
    }
    y = item_loc.y() - kSubmenuBorderSize;
    if (state_.monitor_bounds.width() != 0) {
      pref.set_height(std::min(pref.height(), state_.monitor_bounds.height()));
      if (y + pref.height() > state_.monitor_bounds.bottom())
        y = state_.monitor_bounds.bottom() - pref.height();
      if (y < state_.monitor_bounds.y())
        y = state_.monitor_bounds.y();
    }
  }

  if (state_.monitor_bounds.width() != 0) {
    if (x + pref.width() > state_.monitor_bounds.right())
      x = state_.monitor_bounds.right() - pref.width();
    if (x < state_.monitor_bounds.x())
      x = state_.monitor_bounds.x();
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

// static
int MenuController::MenuDepth(MenuItemView* item) {
  if (!item)
    return 0;
  return MenuDepth(item->GetParentMenuItem()) + 1;
}

void MenuController::IncrementSelection(int delta) {
  MenuItemView* item = pending_state_.item;
  DCHECK(item);
  if (pending_state_.submenu_open && item->HasSubmenu() &&
      item->GetSubmenu()->IsShowing()) {
    // A menu is selected and open, but none of its children are selected,
    // select the first menu item.
    if (item->GetSubmenu()->GetMenuItemCount()) {
      SetSelection(item->GetSubmenu()->GetMenuItemAt(0), false, false);
      ScrollToVisible(item->GetSubmenu()->GetMenuItemAt(0));
      return; // return so else case can fall through.
    }
  }
  if (item->GetParentMenuItem()) {
    MenuItemView* parent = item->GetParentMenuItem();
    int parent_count = parent->GetSubmenu()->GetMenuItemCount();
    if (parent_count > 1) {
      for (int i = 0; i < parent_count; ++i) {
        if (parent->GetSubmenu()->GetMenuItemAt(i) == item) {
          int next_index = (i + delta + parent_count) % parent_count;
          ScrollToVisible(parent->GetSubmenu()->GetMenuItemAt(next_index));
          SetSelection(parent->GetSubmenu()->GetMenuItemAt(next_index), false,
                       false);
          break;
        }
      }
    }
  }
}

void MenuController::OpenSubmenuChangeSelectionIfCan() {
  MenuItemView* item = pending_state_.item;
  if (item->HasSubmenu()) {
    if (item->GetSubmenu()->GetMenuItemCount() > 0) {
      SetSelection(item->GetSubmenu()->GetMenuItemAt(0), false, true);
    } else {
      // No menu items, just show the sub-menu.
      SetSelection(item, true, true);
    }
  }
}

void MenuController::CloseSubmenu() {
  MenuItemView* item = state_.item;
  DCHECK(item);
  if (!item->GetParentMenuItem())
    return;
  if (item->HasSubmenu() && item->GetSubmenu()->IsShowing()) {
    SetSelection(item, false, true);
  } else if (item->GetParentMenuItem()->GetParentMenuItem()) {
    SetSelection(item->GetParentMenuItem(), false, true);
  }
}

bool MenuController::IsMenuWindow(MenuItemView* item, HWND window) {
  if (!item)
    return false;
  return ((item->HasSubmenu() && item->GetSubmenu()->IsShowing() &&
           item->GetSubmenu()->GetWidget()->GetHWND() == window) ||
           IsMenuWindow(item->GetParentMenuItem(), window));
}

bool MenuController::SelectByChar(wchar_t character) {
  wchar_t char_array[1] = { character };
  wchar_t key = l10n_util::ToLower(char_array)[0];
  MenuItemView* item = pending_state_.item;
  if (!item->HasSubmenu() || !item->GetSubmenu()->IsShowing())
    item = item->GetParentMenuItem();
  DCHECK(item);
  DCHECK(item->HasSubmenu());
  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);
  int menu_item_count = submenu->GetMenuItemCount();
  if (!menu_item_count)
    return false;
  for (int i = 0; i < menu_item_count; ++i) {
    MenuItemView* child = submenu->GetMenuItemAt(i);
    if (child->GetMnemonic() == key && child->IsEnabled()) {
      Accept(child, 0);
      return true;
    }
  }

  // No matching mnemonic, search through items that don't have mnemonic
  // based on first character of the title.
  int first_match = -1;
  bool has_multiple = false;
  int next_match = -1;
  int index_of_item = -1;
  for (int i = 0; i < menu_item_count; ++i) {
    MenuItemView* child = submenu->GetMenuItemAt(i);
    if (!child->GetMnemonic() && child->IsEnabled()) {
      std::wstring lower_title = l10n_util::ToLower(child->GetTitle());
      if (child == pending_state_.item)
        index_of_item = i;
      if (lower_title.length() && lower_title[0] == key) {
        if (first_match == -1)
          first_match = i;
        else
          has_multiple = true;
        if (next_match == -1 && index_of_item != -1 && i > index_of_item)
          next_match = i;
      }
    }
  }
  if (first_match != -1) {
    if (!has_multiple) {
      if (submenu->GetMenuItemAt(first_match)->HasSubmenu()) {
        SetSelection(submenu->GetMenuItemAt(first_match), true, false);
      } else {
        Accept(submenu->GetMenuItemAt(first_match), 0);
        return true;
      }
    } else if (index_of_item == -1 || next_match == -1) {
      SetSelection(submenu->GetMenuItemAt(first_match), false, false);
    } else {
      SetSelection(submenu->GetMenuItemAt(next_match), false, false);
    }
  }
  return false;
}

void MenuController::RepostEvent(SubmenuView* source,
                                 const MouseEvent& event) {
  gfx::Point screen_loc(event.location());
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
  HWND window = WindowFromPoint(screen_loc.ToPOINT());
  if (window) {
#ifdef DEBUG_MENU
    DLOG(INFO) << "RepostEvent on press";
#endif

    // Release the capture.
    SubmenuView* submenu = state_.item->GetRootMenuItem()->GetSubmenu();
    submenu->ReleaseCapture();

    if (submenu->host() && submenu->host()->GetHWND() &&
        GetWindowThreadProcessId(submenu->host()->GetHWND(), NULL) !=
        GetWindowThreadProcessId(window, NULL)) {
      // Even though we have mouse capture, windows generates a mouse event
      // if the other window is in a separate thread. Don't generate an event in
      // this case else the target window can get double events leading to bad
      // behavior.
      return;
    }

    // Convert the coordinates to the target window.
    RECT window_bounds;
    GetWindowRect(window, &window_bounds);
    int window_x = screen_loc.x() - window_bounds.left;
    int window_y = screen_loc.y() - window_bounds.top;

    // Determine whether the click was in the client area or not.
    // NOTE: WM_NCHITTEST coordinates are relative to the screen.
    LRESULT nc_hit_result = SendMessage(window, WM_NCHITTEST, 0,
                                        MAKELPARAM(screen_loc.x(),
                                                   screen_loc.y()));
    const bool in_client_area = (nc_hit_result == HTCLIENT);

    // TODO(sky): this isn't right. The event to generate should correspond
    // with the event we just got. MouseEvent only tells us what is down,
    // which may differ. Need to add ability to get changed button from
    // MouseEvent.
    int event_type;
    if (event.IsLeftMouseButton())
      event_type = in_client_area ? WM_LBUTTONDOWN : WM_NCLBUTTONDOWN;
    else if (event.IsMiddleMouseButton())
      event_type = in_client_area ? WM_MBUTTONDOWN : WM_NCMBUTTONDOWN;
    else if (event.IsRightMouseButton())
      event_type = in_client_area ? WM_RBUTTONDOWN : WM_NCRBUTTONDOWN;
    else
      event_type = 0;  // Unknown mouse press.

    if (event_type) {
      if (in_client_area) {
        PostMessage(window, event_type, event.GetWindowsFlags(),
                    MAKELPARAM(window_x, window_y));
      } else {
        PostMessage(window, WM_NCLBUTTONDOWN, nc_hit_result,
                    MAKELPARAM(window_x, window_y));
      }
    }
  }
}

void MenuController::SetDropMenuItem(
    MenuItemView* new_target,
    MenuDelegate::DropPosition new_position) {
  if (new_target == drop_target_ && new_position == drop_position_)
    return;

  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        NULL, MenuDelegate::DROP_NONE);
  }
  drop_target_ = new_target;
  drop_position_ = new_position;
  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        drop_target_, drop_position_);
  }
}

void MenuController::UpdateScrolling(const MenuPart& part) {
  if (!part.is_scroll() && !scroll_task_.get())
    return;

  if (!scroll_task_.get())
    scroll_task_.reset(new MenuScrollTask());
  scroll_task_->Update(part);
}

void MenuController::StopScrolling() {
  scroll_task_.reset(NULL);
}

}  // namespace views
