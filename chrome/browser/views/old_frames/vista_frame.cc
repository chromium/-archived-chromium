// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/old_frames/vista_frame.h"

#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atltheme.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <limits>

#include "base/gfx/native_theme.h"
#include "base/logging.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/frame_util.h"
#include "chrome/browser/suspend_controller.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/accessibility/view_accessibility.h"
#include "chrome/views/aero_tooltip_manager.h"
#include "chrome/views/background.h"
#include "chrome/views/event.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/views/hwnd_notification_source.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// By how much the toolbar overlaps with the tab strip.
static const int kToolbarOverlapVertOffset = 3;

// How much space on the right is not used for the tab strip (to provide
// separation between the tabs and the window controls).
static const int kTabStripRightHorizOffset = 30;

static const int kResizeCornerSize = 12;
static const int kResizeBorder = 5;
static const int kTitlebarHeight = 14;
static const int kTabShadowSize = 2;

// The line drawn to separate tab end contents.
static const int kSeparationLineHeight = 1;

// OTR image offsets.
static const int kOTRImageHorizMargin = 2;
static const int kOTRImageVertMargin = 2;

// Distributor logo offsets.
static const int kDistributorLogoVerticalOffset = 3;

// The DWM puts a light border around the client area - we need to
// take this border size into account when we reduce its size so that
// we don't draw our content border dropshadow images over the top.
static const int kDwmBorderSize = 1;

// When laying out the tabstrip, we size it such that it fits to the left of
// the window controls. We get the bounds of the window controls by sending a
// message to the window, but Windows answers the question assuming 96 dpi and
// a fairly conventional screen layout (i.e. not rotated etc). So we need to
// hack around this by making sure the tabstrip is at least this amount inset
// from the right side of the window.
static const int kWindowControlsMinOffset = 100;

//static
bool VistaFrame::g_initialized = FALSE;

//static
SkBitmap** VistaFrame::g_bitmaps;

static const int kImageNames[] = { IDR_CONTENT_BOTTOM_CENTER,
                                   IDR_CONTENT_BOTTOM_LEFT_CORNER,
                                   IDR_CONTENT_BOTTOM_RIGHT_CORNER,
                                   IDR_CONTENT_LEFT_SIDE,
                                   IDR_CONTENT_RIGHT_SIDE,
                                   IDR_CONTENT_TOP_CENTER,
                                   IDR_CONTENT_TOP_LEFT_CORNER,
                                   IDR_CONTENT_TOP_RIGHT_CORNER,
};

typedef enum { CT_BOTTOM_CENTER = 0, CT_BOTTOM_LEFT_CORNER,
               CT_BOTTOM_RIGHT_CORNER, CT_LEFT_SIDE, CT_RIGHT_SIDE,
               CT_TOP_CENTER, CT_TOP_LEFT_CORNER, CT_TOP_RIGHT_CORNER };

using ChromeViews::Accelerator;
using ChromeViews::FocusManager;

//static
VistaFrame* VistaFrame::CreateFrame(const gfx::Rect& bounds,
                                    Browser* browser,
                                    bool is_off_the_record) {
  VistaFrame* instance = new VistaFrame(browser);
  instance->Create(NULL, bounds.ToRECT(),
      l10n_util::GetString(IDS_PRODUCT_NAME).c_str());
  instance->InitAfterHWNDCreated();
  instance->SetIsOffTheRecord(is_off_the_record);
  FocusManager::CreateFocusManager(instance->m_hWnd, instance->GetRootView());
  return instance;
}

VistaFrame::VistaFrame(Browser* browser)
    : browser_(browser),
      root_view_(this),
      tabstrip_(NULL),
      active_bookmark_bar_(NULL),
      tab_contents_container_(NULL),
      custom_window_enabled_(false),
      saved_window_placement_(false),
      on_mouse_leave_armed_(false),
      in_drag_session_(false),
      shelf_view_(NULL),
      bookmark_bar_view_(NULL),
      info_bar_view_(NULL),
      is_off_the_record_(false),
      off_the_record_image_(NULL),
      distributor_logo_(NULL),
      ignore_ncactivate_(false),
      should_save_window_placement_(browser->GetType() != BrowserType::BROWSER),
      browser_view_(NULL) {
  InitializeIfNeeded();
}

void VistaFrame::InitializeIfNeeded() {
  if (!g_initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    g_bitmaps = new SkBitmap*[arraysize(kImageNames)];
    for (int i = 0; i < arraysize(kImageNames); i++)
      g_bitmaps[i] = rb.GetBitmapNamed(kImageNames[i]);
    g_initialized = TRUE;
  }
}

VistaFrame::~VistaFrame() {
  DestroyBrowser();
}

// On Vista (unlike on XP), we let the OS render the Windows decor (close
// button, maximize button, etc.). Since the mirroring infrastructure in
// ChromeViews does not rely on HWND flipping, the Windows decor on Vista are
// not mirrored for RTL locales; that is, they appear on the upper right
// instead of on the upper left (see bug http://b/issue?id=1128173).
//
// Due to the above, we need to be careful when positioning the tabstrip and
// the OTR image. The OTR image and the tabstrip are automatically mirrored for
// RTL locales by the mirroring infrastructure. In order to make sure they are
// not mirrored, we flip them manually so make sure they don't overlap the
// Windows decor. Unfortunately, there is no cleaner way to do this because the
// current ChromeViews mirroring API does not allow mirroring the position of
// a subset of child Views; in other words, once a View is mirrored (in our
// case frame_view_), then the positions of all its child Views (including, in
// our case, the OTR image and the tabstrip) are mirrored. Once bug mentioned
// above is fixed, we won't need to perform any manual mirroring.
void VistaFrame::Layout() {
  CRect client_rect;
  GetClientRect(&client_rect);
  int width = client_rect.Width();
  int height = client_rect.Height();

  root_view_.SetBounds(0, 0, width, height);
  frame_view_->SetBounds(0, 0, width, height);

  if (IsTabStripVisible()) {
    tabstrip_->SetVisible(true);
    int tabstrip_x = g_bitmaps[CT_LEFT_SIDE]->width();
    if (is_off_the_record_) {
      off_the_record_image_->SetVisible(true);
      CSize otr_image_size;
      off_the_record_image_->GetPreferredSize(&otr_image_size);
      tabstrip_x += otr_image_size.cx + (2 * kOTRImageHorizMargin);
      gfx::Rect off_the_record_bounds;
      if (IsZoomed()) {
        off_the_record_bounds.SetRect(g_bitmaps[CT_LEFT_SIDE]->width(),
                                      kResizeBorder,
                                      otr_image_size.cx,
                                      tabstrip_->GetPreferredHeight() -
                                      kToolbarOverlapVertOffset + 1);
      } else {
        off_the_record_bounds.SetRect(g_bitmaps[CT_LEFT_SIDE]->width(),
                                      kResizeBorder + kTitlebarHeight +
                                      tabstrip_->GetPreferredHeight() -
                                      otr_image_size.cy -
                                      kToolbarOverlapVertOffset + 1,
                                      otr_image_size.cx,
                                      otr_image_size.cy);
      }

      if (frame_view_->UILayoutIsRightToLeft())
        off_the_record_bounds.set_x(frame_view_->MirroredLeftPointForRect(
            off_the_record_bounds));
      off_the_record_image_->SetBounds(off_the_record_bounds.x(),
                                       off_the_record_bounds.y(),
                                       off_the_record_bounds.width(),
                                       off_the_record_bounds.height());

    }

    // Figure out where the minimize button is for layout purposes.
    TITLEBARINFOEX titlebar_info;
    titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
    SendMessage(WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

    // rgrect[2] refers to the minimize button. min_offset will
    // be the distance between the right side of the window
    // and the minimize button.
    CRect window_rect;
    GetWindowRect(&window_rect);
    int min_offset = window_rect.right - titlebar_info.rgrect[2].left;

    // If we are maxmized, the tab strip will be in line with the window
    // controls, so we need to make sure they don't overlap.
    int zoomed_offset = 0;
    if (distributor_logo_) {
      if(IsZoomed()) {
        zoomed_offset = std::max(min_offset, kWindowControlsMinOffset);

        // Hide the distributor logo if we're zoomed.
        distributor_logo_->SetVisible(false);
      } else {
        CSize distributor_logo_size;
        distributor_logo_->GetPreferredSize(&distributor_logo_size);

        int logo_x;
        // Because of Bug 1128173, our Window controls aren't actually flipped 
        // on Vista, yet all our math and layout presumes that they are.
        if (frame_view_->UILayoutIsRightToLeft())
          logo_x = width - distributor_logo_size.cx;
        else
          logo_x = width - min_offset - distributor_logo_size.cx;

        distributor_logo_->SetVisible(true);
        distributor_logo_->SetBounds(logo_x,
            kDistributorLogoVerticalOffset,
            distributor_logo_size.cx,
            distributor_logo_size.cy);
      }
    }

    gfx::Rect tabstrip_bounds(tabstrip_x,
                              kResizeBorder + (IsZoomed() ?
                                  kDwmBorderSize : kTitlebarHeight),
                              width - tabstrip_x - kTabStripRightHorizOffset -
                              zoomed_offset,
                              tabstrip_->GetPreferredHeight());
    if (frame_view_->UILayoutIsRightToLeft() &&
        (IsZoomed() || is_off_the_record_))
      tabstrip_bounds.set_x(
          frame_view_->MirroredLeftPointForRect(tabstrip_bounds));
    tabstrip_->SetBounds(tabstrip_bounds.x(),
                         tabstrip_bounds.y(),
                         tabstrip_bounds.width(),
                         tabstrip_bounds.height());

    frame_view_->SetContentsOffset(tabstrip_->y() +
                                   tabstrip_->height() -
                                   kToolbarOverlapVertOffset);
  } else {
    tabstrip_->SetBounds(0, 0, 0, 0);
    tabstrip_->SetVisible(false);
    if (is_off_the_record_)
      off_the_record_image_->SetVisible(false);
  }

  int toolbar_bottom;
  if (IsToolBarVisible()) {
    browser_view_->SetVisible(true);
    browser_view_->SetBounds(g_bitmaps[CT_LEFT_SIDE]->width(),
                             tabstrip_->y() + tabstrip_->height() -
                             kToolbarOverlapVertOffset,
                             width - g_bitmaps[CT_LEFT_SIDE]->width() -
                             g_bitmaps[CT_RIGHT_SIDE]->width(),
                             g_bitmaps[CT_TOP_CENTER]->height());
    browser_view_->Layout();
    toolbar_bottom = browser_view_->y() + browser_view_->height();
  } else {
    browser_view_->SetBounds(0, 0, 0, 0);
    browser_view_->SetVisible(false);
    toolbar_bottom = tabstrip_->y() + tabstrip_->height();
  }
  int browser_x, browser_y;
  int browser_w, browser_h;

  if (IsTabStripVisible()) {
    browser_x = g_bitmaps[CT_LEFT_SIDE]->width();
    browser_y = toolbar_bottom;
    browser_w = width - g_bitmaps[CT_LEFT_SIDE]->width() -
        g_bitmaps[CT_RIGHT_SIDE]->width();
    browser_h = height - browser_y - g_bitmaps[CT_BOTTOM_CENTER]->height();
  } else {
    browser_x = 0;
    browser_y = toolbar_bottom;
    browser_w = width;
    browser_h = height;
  }

  CSize preferred_size;
  if (shelf_view_) {
    shelf_view_->GetPreferredSize(&preferred_size);
    shelf_view_->SetBounds(browser_x,
                           height - g_bitmaps[CT_BOTTOM_CENTER]->height() -
                           preferred_size.cy,
                           browser_w,
                           preferred_size.cy);
    browser_h -= preferred_size.cy;
  }

  CSize bookmark_bar_size;
  CSize info_bar_size;

  if (bookmark_bar_view_.get())
    bookmark_bar_view_->GetPreferredSize(&bookmark_bar_size);

  if (info_bar_view_)
    info_bar_view_->GetPreferredSize(&info_bar_size);

  // If we're showing a bookmarks bar in the new tab page style and we
  // have an infobar showing, we need to flip them.
  if (info_bar_view_ &&
      bookmark_bar_view_.get() &&
      bookmark_bar_view_->IsNewTabPage() &&
      !bookmark_bar_view_->IsAlwaysShown()) {
    info_bar_view_->SetBounds(browser_x,
                              browser_y,
                              browser_w,
                              info_bar_size.cy);
    browser_h -= info_bar_size.cy;

    browser_y += info_bar_size.cy - kSeparationLineHeight;

    bookmark_bar_view_->SetBounds(browser_x,
                                  browser_y,
                                  browser_w,
                                  bookmark_bar_size.cy);
    browser_h -= bookmark_bar_size.cy - kSeparationLineHeight;
    browser_y += bookmark_bar_size.cy;
  } else {
    if (bookmark_bar_view_.get()) {
      // We want our bookmarks bar to be responsible for drawing its own
      // separator, so we let it overlap ours.
      browser_y -= kSeparationLineHeight;

      bookmark_bar_view_->SetBounds(browser_x,
                                    browser_y,
                                    browser_w,
                                    bookmark_bar_size.cy);
      browser_h -= bookmark_bar_size.cy - kSeparationLineHeight;
      browser_y += bookmark_bar_size.cy;
    }

    if (info_bar_view_) {
      info_bar_view_->SetBounds(browser_x,
                                browser_y,
                                browser_w,
                                info_bar_size.cy);
      browser_h -= info_bar_size.cy;
      browser_y += info_bar_size.cy;
    }
  }

  // While our OnNCCalcSize handler does a good job of covering most of the
  // cases where we need to do this, it unfortunately doesn't cover the
  // case where we're returning from maximized mode.
  ResetDWMFrame();

  tab_contents_container_->SetBounds(browser_x,
                                     browser_y,
                                     browser_w,
                                     browser_h);

  browser_view_->LayoutStatusBubble(browser_y + browser_h);

  frame_view_->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
//
// BrowserWindow implementation
//
////////////////////////////////////////////////////////////////////////////////

void VistaFrame::Init() {
  FrameUtil::RegisterBrowserWindow(this);

  // Link the HWND with its root view so we can retrieve the RootView from the
  // HWND for automation purposes.
  ChromeViews::SetRootViewForHWND(m_hWnd, &root_view_);

  frame_view_ = new VistaFrameView(this);
  root_view_.AddChildView(frame_view_);
  root_view_.SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  frame_view_->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  browser_view_ = new BrowserView(this, browser_, NULL, NULL);
  frame_view_->AddChildView(browser_view_);

  tabstrip_ = CreateTabStrip(browser_);
  tabstrip_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  frame_view_->AddChildView(tabstrip_);

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  if (is_off_the_record_) {
    off_the_record_image_ = new ChromeViews::ImageView();
    frame_view_->AddViewToDropList(off_the_record_image_);
    SkBitmap* otr_icon = rb.GetBitmapNamed(IDR_OTR_ICON);
    off_the_record_image_->SetImage(*otr_icon);
    off_the_record_image_->SetTooltipText(
        l10n_util::GetString(IDS_OFF_THE_RECORD_TOOLTIP));
    off_the_record_image_->SetVerticalAlignment(
        ChromeViews::ImageView::LEADING);
    frame_view_->AddChildView(off_the_record_image_);
  }

  SkBitmap* image = rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO);
  if (!image->isNull()) {
    distributor_logo_ = new ChromeViews::ImageView();
    frame_view_->AddViewToDropList(distributor_logo_);
    distributor_logo_->SetImage(image);
    frame_view_->AddChildView(distributor_logo_);
  }

  tab_contents_container_ = new TabContentsContainerView();
  frame_view_->AddChildView(tab_contents_container_);

  // Add the task manager item to the system menu before the last entry.
  task_manager_label_text_ = l10n_util::GetString(IDS_TASKMANAGER);
  HMENU system_menu = ::GetSystemMenu(m_hWnd, FALSE);
  int index = ::GetMenuItemCount(system_menu) - 1;
  if (index < 0) {
    NOTREACHED();
    // Paranoia check.
    index = 0;
  }

  // First we add the separator.
  MENUITEMINFO menu_info;
  memset(&menu_info, 0, sizeof(MENUITEMINFO));
  menu_info.cbSize = sizeof(MENUITEMINFO);
  menu_info.fMask = MIIM_FTYPE;
  menu_info.fType = MFT_SEPARATOR;
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);
  // Then the actual menu.
  menu_info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
  menu_info.fType = MFT_STRING;
  menu_info.fState = MFS_ENABLED;
  menu_info.wID = IDC_TASKMANAGER;
  menu_info.dwTypeData = const_cast<wchar_t*>(task_manager_label_text_.c_str());
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);

  // Register accelerators.
  HACCEL accelerators_table = AtlLoadAccelerators(IDR_MAINFRAME);
  DCHECK(accelerators_table);
  FrameUtil::LoadAccelerators(this, accelerators_table, this);

  ShelfVisibilityChanged();
  root_view_.OnViewContainerCreated();
  Layout();
}

TabStrip* VistaFrame::CreateTabStrip(Browser* browser) {
  return new TabStrip(browser->tabstrip_model());
}

void VistaFrame::Show(int command, bool adjust_to_fit) {
  if (adjust_to_fit)
    win_util::AdjustWindowToFit(*this);
  ::ShowWindow(*this, command);
}

// This is called when we receive WM_ENDSESSION. In Vista the we have 5 seconds
// or will be forcefully terminated if we get stuck servicing this message and
// not pump the final messages.
void VistaFrame::OnEndSession(BOOL ending, UINT logoff) {
  tabstrip_->AbortActiveDragSession();
  FrameUtil::EndSession();
}

// Note: called directly by the handler macros to handle WM_CLOSE messages.
void VistaFrame::Close() {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_->IsDragSessionActive())
    return;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
      return;

  // We call this here so that the window position gets saved before moving
  // the window into hyperspace.
  if (!saved_window_placement_ && should_save_window_placement_ && browser_) {
    browser_->SaveWindowPlacement();
    browser_->SaveWindowPlacementToDatabase();
    saved_window_placement_ = true;
  }

  if (browser_ && !browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the window (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down.  When the tab strip is empty we'll be called back recursively.
    // NOTE: Don't use ShowWindow(SW_HIDE) here, otherwise end session blocks
    // here.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
    browser_->OnWindowClosing();
  } else {
    // Empty tab strip, it's now safe to clean-up.
    root_view_.OnViewContainerDestroyed();

    NotificationService::current()->Notify(
        NOTIFY_WINDOW_CLOSED, Source<HWND>(m_hWnd),
        NotificationService::NoDetails());

    DestroyWindow();
  }
}

void* VistaFrame::GetPlatformID() {
  return reinterpret_cast<void*>(m_hWnd);
}

void VistaFrame::SetAcceleratorTable(
    std::map<Accelerator, int>* accelerator_table) {
  accelerator_table_.reset(accelerator_table);
}

bool VistaFrame::GetAccelerator(int cmd_id,
                                ChromeViews::Accelerator* accelerator) {
  std::map<ChromeViews::Accelerator, int>::iterator it =
      accelerator_table_->begin();
  for (; it != accelerator_table_->end(); ++it) {
    if(it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}

bool VistaFrame::AcceleratorPressed(const Accelerator& accelerator) {
  DCHECK(accelerator_table_.get());
  std::map<Accelerator, int>::const_iterator iter =
      accelerator_table_->find(accelerator);
  DCHECK(iter != accelerator_table_->end());

  int command_id = iter->second;
  if (browser_->SupportsCommand(command_id) &&
      browser_->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

gfx::Rect VistaFrame::GetNormalBounds() {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool ret = !!GetWindowPlacement(&wp);
  DCHECK(ret);
  return gfx::Rect(wp.rcNormalPosition);
}

bool VistaFrame::IsMaximized() {
  return !!IsZoomed();
}

gfx::Rect VistaFrame::GetBoundsForContentBounds(const gfx::Rect content_rect) {
  if (tab_contents_container_->x() == 0 &&
      tab_contents_container_->width() == 0) {
    Layout();
  }

  CPoint p(0, 0);
  ChromeViews::View::ConvertPointToViewContainer(tab_contents_container_, &p);
  CRect bounds;
  GetBounds(&bounds, true);

  gfx::Rect r;
  r.set_x(content_rect.x() - p.x);
  r.set_y(content_rect.y() - p.y);
  r.set_width(p.x + content_rect.width() +
              (bounds.Width() - (p.x + tab_contents_container_->width())));
  r.set_height(p.y + content_rect.height() +
               (bounds.Height() - (p.y +
                                   tab_contents_container_->height())));
  return r;
}

void VistaFrame::InfoBubbleShowing() {
  ignore_ncactivate_ = true;
}

ToolbarStarToggle* VistaFrame::GetStarButton() const {
  return NULL;
}

LocationBarView* VistaFrame::GetLocationBarView() const {
  return NULL;
}

GoButton* VistaFrame::GetGoButton() const {
  return NULL;
}

BookmarkBarView* VistaFrame::GetBookmarkBarView() {
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!current_tab || !current_tab->profile())
    return NULL;

  if (!bookmark_bar_view_.get()) {
    bookmark_bar_view_.reset(new BookmarkBarView(current_tab->profile(),
                                                 browser_));
    bookmark_bar_view_->SetParentOwned(false);
  } else {
    bookmark_bar_view_->SetProfile(current_tab->profile());
  }
  bookmark_bar_view_->SetPageNavigator(current_tab);
  return bookmark_bar_view_.get();
}

BrowserView* VistaFrame::GetBrowserView() const {
  return browser_view_;
}

void VistaFrame::UpdateToolbar(TabContents* contents,
                               bool should_restore_state) {
}

void VistaFrame::ProfileChanged(Profile* profile) {
}

void VistaFrame::FocusToolbar() {
}

bool VistaFrame::IsBookmarkBarVisible() const {
  if (!bookmark_bar_view_.get())
    return false;

  if (bookmark_bar_view_->IsNewTabPage() || bookmark_bar_view_->IsAnimating())
    return true;

  CSize sz;
  bookmark_bar_view_->GetPreferredSize(&sz);
  // 1 is the minimum in GetPreferredSize for the bookmark bar.
  return sz.cy > 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Events
//
////////////////////////////////////////////////////////////////////////////////

LRESULT VistaFrame::OnSettingChange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                                    BOOL& handled) {
  // Set this to false, if we actually handle the message, we'll set it to
  // true below.
  handled = FALSE;
  if (w_param != SPI_SETWORKAREA)
    return 0; // Return value is effectively ignored in atlwin.h.

  win_util::AdjustWindowToFit(m_hWnd);
  handled = TRUE;
  return 0;  // Return value is effectively ignored in atlwin.h.
}

LRESULT VistaFrame::OnNCActivate(BOOL param) {
  if (ignore_ncactivate_) {
    ignore_ncactivate_ = false;
    return DefWindowProc(WM_NCACTIVATE, TRUE, 0);
  }
  SetMsgHandled(false);
  return 0;
}

BOOL VistaFrame::OnPowerBroadcast(DWORD power_event, DWORD data) {
  if (PBT_APMSUSPEND == power_event) {
    SuspendController::OnSuspend(browser_->profile());
    return TRUE;
  } else if (PBT_APMRESUMEAUTOMATIC == power_event) {
    SuspendController::OnResume(browser_->profile());
    return TRUE;
  }

  SetMsgHandled(FALSE);
  return FALSE;
}

void VistaFrame::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
}

void VistaFrame::OnMouseButtonDown(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  if (ProcessMousePressed(pt, flags, FALSE)) {
    SetMsgHandled(TRUE);
  } else {
    SetMsgHandled(FALSE);
  }
}

void VistaFrame::OnMouseButtonUp(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  if (in_drag_session_) {
    ProcessMouseReleased(pt, flags);
  }
}

void VistaFrame::OnMouseButtonDblClk(UINT flags, const CPoint& pt) {
  if (ProcessMousePressed(pt, flags, TRUE)) {
    SetMsgHandled(TRUE);
  } else {
    SetMsgHandled(FALSE);
  }
}

void VistaFrame::OnLButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_LBUTTON, pt);
}

void VistaFrame::OnMButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_MBUTTON, pt);
}

void VistaFrame::OnRButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_RBUTTON, pt);
}

void VistaFrame::OnNCMButtonDown(UINT flags, const CPoint& pt) {
  // The point is in screen coordinate system so we need to convert
  CRect window_rect;
  GetWindowRect(&window_rect);
  CPoint point(pt);
  point.x -= window_rect.left;
  point.y -= window_rect.top;

  // Yes we need to add MK_MBUTTON. Windows doesn't include it
  OnMouseButtonDown(flags | MK_MBUTTON, point);
}

void VistaFrame::OnNCRButtonDown(UINT flags, const CPoint& pt) {
  if (flags == HTCAPTION) {
    HMENU system_menu = ::GetSystemMenu(*this, FALSE);
    int id = ::TrackPopupMenu(system_menu,
                              TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                              pt.x,
                              pt.y,
                              0,
                              *this,
                              NULL);
    if (id)
      ::SendMessage(*this,
                    WM_SYSCOMMAND,
                    id,
                    0);
  } else {
    SetMsgHandled(FALSE);
  }
}

void VistaFrame::OnMouseMove(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  if (in_drag_session_) {
    ProcessMouseDragged(pt, flags);
  } else {
    ArmOnMouseLeave();
    ProcessMouseMoved(pt, flags);
  }
}

void VistaFrame::OnMouseLeave() {
  if (m_hWnd == NULL) {
    return;
  }

  ProcessMouseExited();
  on_mouse_leave_armed_ = false;
}

LRESULT VistaFrame::OnGetObject(UINT uMsg, WPARAM w_param, LPARAM object_id) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Accessibility readers will send an OBJID_CLIENT message
  if (OBJID_CLIENT == object_id) {
    // If our MSAA root is already created, reuse that pointer. Otherwise,
    // create a new one.
    if (!accessibility_root_) {
      CComObject<ViewAccessibility>* instance = NULL;

      HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);
      DCHECK(SUCCEEDED(hr));

      if (!instance) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      CComPtr<IAccessible> accessibility_instance(instance);

      if (!SUCCEEDED(instance->Initialize(&root_view_))) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      // All is well, assign the temp instance to the class smart pointer
      accessibility_root_.Attach(accessibility_instance.Detach());

      if (!accessibility_root_) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      // Notify that an instance of IAccessible was allocated for m_hWnd
      ::NotifyWinEvent(EVENT_OBJECT_CREATE, m_hWnd, OBJID_CLIENT,
                       CHILDID_SELF);
    }

    // Create a reference to ViewAccessibility that MSAA will marshall
    // to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(accessibility_root_));
  }
  return reference_result;
}

void VistaFrame::OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags) {
  ChromeViews::KeyEvent event(ChromeViews::Event::ET_KEY_PRESSED, c,
                              rep_cnt, flags);
  root_view_.ProcessKeyEvent(event);
}

void VistaFrame::OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags) {
  ChromeViews::KeyEvent event(ChromeViews::Event::ET_KEY_RELEASED, c,
                              rep_cnt, flags);
  root_view_.ProcessKeyEvent(event);
}

LRESULT VistaFrame::OnAppCommand(
    HWND w_param, short app_command, WORD device, int keystate) {
  if (browser_ && !browser_->ExecuteWindowsAppCommand(app_command))
    SetMsgHandled(FALSE);
  return 0;
}

void VistaFrame::OnCommand(UINT notification_code,
                           int command_id,
                           HWND window) {
  if (browser_ && browser_->SupportsCommand(command_id)) {
    browser_->ExecuteCommand(command_id);
  } else {
    SetMsgHandled(FALSE);
  }
}

void VistaFrame::OnSysCommand(UINT notification_code, CPoint click) {
  switch (notification_code) {
    case IDC_TASKMANAGER:
      if (browser_)
        browser_->ExecuteCommand(IDC_TASKMANAGER);
      break;
    default:
      // Use the default implementation for any other command.
      SetMsgHandled(FALSE);
      break;
  }
}

void VistaFrame::OnMove(const CSize& size) {
  if (!saved_window_placement_ && should_save_window_placement_ )
    browser_->SaveWindowPlacementToDatabase();

  browser_->WindowMoved();
}

void VistaFrame::OnMoving(UINT param, LPRECT new_bounds) {
  // We want to let the browser know that the window moved so that it can
  // update the positions of any dependent WS_POPUPs
  browser_->WindowMoved();
}

void VistaFrame::OnSize(UINT param, const CSize& size) {
  Layout();

  if (root_view_.NeedsPainting(false)) {
    PaintNow(root_view_.GetScheduledPaintRect());
  }

  if (!saved_window_placement_ && should_save_window_placement_)
    browser_->SaveWindowPlacementToDatabase();
}

void VistaFrame::OnFinalMessage(HWND hwnd) {
  delete this;
}

void VistaFrame::OnNCLButtonDown(UINT flags, const CPoint& pt) {
  SetMsgHandled(false);
}

LRESULT VistaFrame::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // By default the client side is set to the window size which is what
  // we want.
  if (w_param == TRUE) {
    // Calculate new NCCALCSIZE_PARAMS based on custom NCA inset.
    NCCALCSIZE_PARAMS *pncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);

    // Hack necessary to stop black background flicker, we cut out
    // resizeborder here to save us from having to do too much
    // addition and subtraction in Layout(). We don't cut off the
    // top + titlebar as that prevents the window controls from
    // highlighting.
    pncsp->rgrc[0].left   = pncsp->rgrc[0].left   + kResizeBorder;
    pncsp->rgrc[0].right  = pncsp->rgrc[0].right  - kResizeBorder;
    pncsp->rgrc[0].bottom = pncsp->rgrc[0].bottom - kResizeBorder;

    // We need to reset the frame, as Vista resets it whenever it changes
    // composition modes (and NCCALCSIZE is the closest thing we get to
    // a reliable message about the change).
    ResetDWMFrame();

    SetMsgHandled(TRUE);
    return 0;
  }

  SetMsgHandled(FALSE);
  return 0;
}

LRESULT VistaFrame::OnNCHitTest(const CPoint& pt) {
  SetMsgHandled(TRUE);

  // Test the caption buttons
  LRESULT l_res;
  BOOL dwm_processed = DwmDefWindowProc(m_hWnd,
                                        WM_NCHITTEST,
                                        0,
                                        (LPARAM)MAKELONG(pt.x, pt.y),
                                        &l_res);

  if (dwm_processed) {
    return l_res;
  }

  CRect cr;
  GetBounds(&cr, false);

  CPoint tab_pt(pt);
  ChromeViews::View::ConvertPointToView(NULL, tabstrip_, &tab_pt);

  // If we are over the tabstrip
  if (tab_pt.x > 0 && tab_pt.y >= kTabShadowSize &&
      tab_pt.x < tabstrip_->width() &&
      tab_pt.y < tabstrip_->height()) {
    ChromeViews::View* v = tabstrip_->GetViewForPoint(tab_pt);
    if (v == tabstrip_)
      return HTCAPTION;

    // If the view under mouse is a tab, check if the tab strip allows tab
    // dragging or not. If not, return caption to get window dragging.
    if (v->GetClassName() == Tab::kTabClassName &&
        !tabstrip_->HasAvailableDragActions())
      return HTCAPTION;

    return HTCLIENT;
  }

  CPoint p(pt);
  CRect r;
  GetWindowRect(&r);

  // Convert from screen to window coordinates
  p.x -= r.left;
  p.y -= r.top;

  if (p.x < kResizeBorder + g_bitmaps[CT_LEFT_SIDE]->width()) {
    if (p.y < kResizeCornerSize) {
      return HTTOPLEFT;
    } else if (p.y >= (r.Height() - kResizeCornerSize)) {
      return HTBOTTOMLEFT;
    } else {
      return HTLEFT;
    }
    // BOTTOM_LEFT / TOP_LEFT horizontal extensions
  } else if (p.x < kResizeCornerSize) {
    if (p.y < kResizeBorder) {
      return HTTOPLEFT;
    } else if (p.y >= (r.Height() - kResizeBorder)) {
      return HTBOTTOMLEFT;
    }
    // EAST / BOTTOMRIGHT / TOPRIGHT edge
  } else if (p.x >= (r.Width() - kResizeBorder -
             g_bitmaps[CT_RIGHT_SIDE]->width())) {
    if (p.y < kResizeCornerSize) {
      return HTTOPRIGHT;
    } else if (p.y >= (r.Height() - kResizeCornerSize)) {
      return HTBOTTOMRIGHT;
    } else {
      return HTRIGHT;
    }
    // EAST / BOTTOMRIGHT / TOPRIGHT horizontal extensions
  } else if (p.x >= (r.Width() - kResizeCornerSize)) {
    if (p.y < kResizeBorder) {
      return HTTOPRIGHT;
    } else if (p.y >= (r.Height() - kResizeBorder)) {
      return HTBOTTOMRIGHT;
    }
    // TOP edge
  } else if (p.y < kResizeBorder) {
    return HTTOP;
    // BOTTOM edge
  } else if (p.y >= (r.Height() - kResizeBorder -
             g_bitmaps[CT_BOTTOM_CENTER]->height())) {
    return HTBOTTOM;
  }

  if (p.y <= tabstrip_->y() + tabstrip_->height()) {
    return HTCAPTION;
  }

  SetMsgHandled(FALSE);
  return 0;
}

void VistaFrame::OnActivate(UINT n_state, BOOL is_minimized, HWND other) {
  if (FrameUtil::ActivateAppModalDialog(browser_))
    return;

  // Enable our custom window if we haven't already (this works in combination
  // with our NCCALCSIZE handler).
  if (!custom_window_enabled_) {
    RECT rcClient;
    ::GetWindowRect(m_hWnd, &rcClient);
    ::SetWindowPos(
        m_hWnd,
        NULL,
        rcClient.left, rcClient.top,
        rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
        SWP_FRAMECHANGED);
    custom_window_enabled_ = true;

    // We need to fire this here as well as in OnNCCalcSize, as that function
    // does not fire at the right time for this to work when opening the
    // window.
    ResetDWMFrame();
  }

  SetMsgHandled(FALSE);
  if (!is_minimized)
    browser_->WindowActivationChanged(n_state != WA_INACTIVE);
}

int VistaFrame::OnMouseActivate(CWindow wndTopLevel, UINT nHitTest,
                                UINT message) {
  return FrameUtil::ActivateAppModalDialog(browser_) ? MA_NOACTIVATEANDEAT
                                                     : MA_ACTIVATE;
}

void VistaFrame::OnPaint(HDC dc) {
  // Warning: on Vista the ChromeCanvasPaint *must* use an opaque flag of true
  // so that ChromeCanvasPaint performs a BitBlt and not an alpha blend.
  //
  root_view_.OnPaint(*this);
}

LRESULT VistaFrame::OnEraseBkgnd(HDC dc) {
  SetMsgHandled(TRUE);
  return 0;
}

void VistaFrame::ArmOnMouseLeave() {
  if (!on_mouse_leave_armed_) {
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = *this;
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
    on_mouse_leave_armed_ = TRUE;
  }
}

void VistaFrame::OnCaptureChanged(HWND hwnd) {
  if (in_drag_session_)
    root_view_.ProcessMouseDragCanceled();
  in_drag_session_ = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// View events propagation
//
////////////////////////////////////////////////////////////////////////////////

bool VistaFrame::ProcessMousePressed(const CPoint& pt, UINT flags,
                                     bool dbl_click) {
  using namespace ChromeViews;
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           pt.x,
                           pt.y,
                           (dbl_click ? MouseEvent::EF_IS_DOUBLE_CLICK : 0) |
                           Event::ConvertWindowsFlags(flags));
  if (root_view_.OnMousePressed(mouse_pressed)) {
    // If an additional button is pressed during a drag session we don't want
    // to call SetCapture() again as it will result in no more events.
    if (!in_drag_session_) {
      in_drag_session_ = true;
      SetCapture();
    }
    return TRUE;
  }
  return FALSE;
}

void VistaFrame::ProcessMouseDragged(const CPoint& pt, UINT flags) {
  using namespace ChromeViews;
  MouseEvent drag_event(Event::ET_MOUSE_DRAGGED,
                        pt.x,
                        pt.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseDragged(drag_event);
}

void VistaFrame::ProcessMouseReleased(const CPoint& pt, UINT flags) {
  using namespace ChromeViews;
  if (in_drag_session_) {
    in_drag_session_ = false;
    ReleaseCapture();
  }
  MouseEvent mouse_released(Event::ET_MOUSE_RELEASED,
                            pt.x,
                            pt.y,
                            Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseReleased(mouse_released, false);
}

void VistaFrame::ProcessMouseMoved(const CPoint &pt, UINT flags) {
  using namespace ChromeViews;
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                        pt.x,
                        pt.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseMoved(mouse_move);
}

void VistaFrame::ProcessMouseExited() {
  root_view_.ProcessOnMouseExited();
}

////////////////////////////////////////////////////////////////////////////////
//
// ChromeViews::ViewContainer
//
////////////////////////////////////////////////////////////////////////////////

void VistaFrame::GetBounds(CRect *out, bool including_frame) const {
  if (including_frame) {
    GetWindowRect(out);
  } else {
    GetClientRect(out);

    POINT p = {0, 0};
    ::ClientToScreen(*this, &p);

    out->left += p.x;
    out->top += p.y;
    out->right += p.x;
    out->bottom += p.y;
  }
}

void VistaFrame::MoveToFront(bool should_activate) {
  int flags = SWP_NOMOVE | SWP_NOSIZE;
  if (!should_activate) {
    flags |= SWP_NOACTIVATE;
  }
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, flags);
  SetForegroundWindow(*this);
}

HWND VistaFrame::GetHWND() const {
  return m_hWnd;
}

void VistaFrame::PaintNow(const CRect& update_rect) {
  if (!update_rect.IsRectNull() && IsVisible()) {
    RedrawWindow(update_rect,
                 NULL,
                 RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
  }
}

ChromeViews::RootView* VistaFrame::GetRootView() {
  return const_cast<ChromeViews::RootView*>(&root_view_);
}

bool VistaFrame::IsVisible() {
  return !!::IsWindowVisible(*this);
}

bool VistaFrame::IsActive() {
  return win_util::IsWindowActive(*this);
}

////////////////////////////////////////////////////////////////////////////////
//
// VistaFrameView
//
////////////////////////////////////////////////////////////////////////////////
void VistaFrame::VistaFrameView::PaintContentsBorder(ChromeCanvas* canvas,
                                                     int x,
                                                     int y,
                                                     int w,
                                                     int h) {
  if (!parent_->IsTabStripVisible())
    return;
  int ro, bo;
  canvas->DrawBitmapInt(*g_bitmaps[CT_TOP_LEFT_CORNER], x, y);
  canvas->TileImageInt(*g_bitmaps[CT_TOP_CENTER],
                       x + g_bitmaps[CT_TOP_LEFT_CORNER]->width(),
                       y,
                       w - g_bitmaps[CT_TOP_LEFT_CORNER]->width() -
                       g_bitmaps[CT_TOP_RIGHT_CORNER]->width(),
                       g_bitmaps[CT_TOP_CENTER]->height());
  ro = x + w - g_bitmaps[CT_TOP_RIGHT_CORNER]->width();
  canvas->DrawBitmapInt(*g_bitmaps[CT_TOP_RIGHT_CORNER], ro, y);
  canvas->TileImageInt(*g_bitmaps[CT_RIGHT_SIDE],
                       ro,
                       y + g_bitmaps[CT_TOP_RIGHT_CORNER]->height(),
                       g_bitmaps[CT_RIGHT_SIDE]->width(),
                       h - (g_bitmaps[CT_TOP_RIGHT_CORNER]->height() +
                            g_bitmaps[CT_BOTTOM_RIGHT_CORNER]->height()));
  bo = y + h - g_bitmaps[CT_BOTTOM_RIGHT_CORNER]->height();
  canvas->DrawBitmapInt(*g_bitmaps[CT_BOTTOM_RIGHT_CORNER],
                        x + w - g_bitmaps[CT_BOTTOM_RIGHT_CORNER]->width(),
                        bo);

  canvas->TileImageInt(*g_bitmaps[CT_BOTTOM_CENTER],
                       x + g_bitmaps[CT_BOTTOM_LEFT_CORNER]->width(),
                       bo,
                       w - (g_bitmaps[CT_BOTTOM_LEFT_CORNER]->width() +
                            g_bitmaps[CT_BOTTOM_RIGHT_CORNER]->width()),
                       g_bitmaps[CT_BOTTOM_CENTER]->height());
  canvas->DrawBitmapInt(*g_bitmaps[CT_BOTTOM_LEFT_CORNER], x, bo);
  canvas->TileImageInt(*g_bitmaps[CT_LEFT_SIDE],
                       x,
                       y + g_bitmaps[CT_TOP_LEFT_CORNER]->height(),
                       g_bitmaps[CT_LEFT_SIDE]->width(),
                       h - (g_bitmaps[CT_TOP_LEFT_CORNER]->height() +
                            g_bitmaps[CT_BOTTOM_LEFT_CORNER]->height()));
}

void VistaFrame::VistaFrameView::Paint(ChromeCanvas* canvas) {
  canvas->save();

  // When painting the border, exclude the contents area. This will prevent the
  // border bitmaps (which might be larger than the visible area) from coming
  // into the content area when there is no tab painted yet.
  int x = parent_->tab_contents_container_->x();
  int y = parent_->tab_contents_container_->y();
  SkRect clip;
  clip.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + parent_->tab_contents_container_->width()),
           SkIntToScalar(y + parent_->tab_contents_container_->height()));
  canvas->clipRect(clip, SkRegion::kDifference_Op);

  PaintContentsBorder(canvas,
                      0,
                      contents_offset_,
                      width(),
                      height() - contents_offset_);

  canvas->restore();
}

void VistaFrame::VistaFrameView::SetContentsOffset(int o) {
  contents_offset_ = o;
}

bool VistaFrame::VistaFrameView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_CLIENT;
  return true;
}

bool VistaFrame::VistaFrameView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    name->assign(accessible_name_);
    return true;
  }
  return false;
}

void VistaFrame::VistaFrameView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

// Helper function to extract the min/max x-coordinate as well as the max
// y coordinate from the TITLEBARINFOEX struct at the specified index.
static void UpdatePosition(const TITLEBARINFOEX& info,
                           int element,
                           int* min_x,
                           int* max_x,
                           int* max_y) {
  if ((info.rgstate[element] & (STATE_SYSTEM_INVISIBLE |
                                STATE_SYSTEM_OFFSCREEN |
                                STATE_SYSTEM_UNAVAILABLE)) == 0) {
    *min_x = std::min(*min_x, static_cast<int>(info.rgrect[element].left));
    *max_x = std::max(*max_x, static_cast<int>(info.rgrect[element].right));
    *max_y = std::max(*max_y, static_cast<int>(info.rgrect[element].bottom));
  }
}

bool VistaFrame::VistaFrameView::ShouldForwardToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  if (!FrameView::ShouldForwardToTabStrip(event))
    return false;

  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(parent_->m_hWnd, WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  int min_x = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();

  // 2 is the minimize button.
  UpdatePosition(titlebar_info, 2, &min_x, &max_x, &max_y);
  // 3 is the maximize button.
  UpdatePosition(titlebar_info, 3, &min_x, &max_x, &max_y);
  // 5 is the close button.
  UpdatePosition(titlebar_info, 5, &min_x, &max_x, &max_y);

  if (min_x != std::numeric_limits<int>::max() &&
      max_x != std::numeric_limits<int>::min() &&
      max_y != std::numeric_limits<int>::min()) {
    CPoint screen_drag_point(event.x(), event.y());
    ConvertPointToScreen(this, &screen_drag_point);
    if (screen_drag_point.x >= min_x && screen_drag_point.x <= max_x &&
        screen_drag_point.y <= max_y) {
      return false;
    }
  }
  return true;
}

LRESULT VistaFrame::OnMouseRange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                                 BOOL& handled) {
  // Tooltip handling is broken in Vista when using custom frames, so
  // we have to implement a lot of this ourselves.
  tooltip_manager_->OnMouse(u_msg, w_param, l_param);
  handled = FALSE;

  return 0;
}

LRESULT VistaFrame::OnNotify(int w_param, NMHDR* l_param) {
  bool handled;
  LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
  SetMsgHandled(handled);
  return result;
}

ChromeViews::TooltipManager* VistaFrame::GetTooltipManager() {
  return tooltip_manager_.get();
}

StatusBubble* VistaFrame::GetStatusBubble() {
  return NULL;
}

void VistaFrame::InitAfterHWNDCreated() {
  tooltip_manager_.reset(new ChromeViews::AeroTooltipManager(this, m_hWnd));
}

void VistaFrame::ResetDWMFrame() {
  if (IsTabStripVisible()) {
    // Note: we don't use DwmEnableBlurBehindWindow because any region not
    // included in the glass region is composited source over. This means
    // that anything drawn directly with GDI appears fully transparent.
    //
    // We want this region to extend past our content border images, as they
    // may be partially-transparent.
    MARGINS margins = {kDwmBorderSize +
                       g_bitmaps[CT_TOP_LEFT_CORNER]->width(),
                       kDwmBorderSize +
                       g_bitmaps[CT_TOP_RIGHT_CORNER]->width(),
                       kDwmBorderSize +
                       IsToolBarVisible() ?
                         browser_view_->y() + kToolbarOverlapVertOffset :
                         tabstrip_->height(),
                       kDwmBorderSize +
                       g_bitmaps[CT_BOTTOM_CENTER]->height()};

    DwmExtendFrameIntoClientArea(*this, &margins);
  }
}

void VistaFrame::ShelfVisibilityChanged() {
  ShelfVisibilityChangedImpl(browser_->GetSelectedTabContents());
}

void VistaFrame::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    tab_contents_container_->set_fast_resize(true);
    ShelfVisibilityChanged();
    tab_contents_container_->set_fast_resize(false);
  } else {
    ShelfVisibilityChanged();
    tab_contents_container_->UpdateHWNDBounds();
  }
}

bool VistaFrame::UpdateChildViewAndLayout(ChromeViews::View* new_view,
                                          ChromeViews::View** view) {
  DCHECK(view);
  if (*view == new_view) {
    // The views haven't changed, if the views pref changed schedule a layout.
    if (new_view) {
      CSize pref_size;
      new_view->GetPreferredSize(&pref_size);
      if (pref_size.cy != new_view->height())
        return true;
    }
    return false;
  }

  // The views differ, and one may be null (but not both). Remove the old
  // view (if it non-null), and add the new one (if it is non-null). If the
  // height has changed, schedule a layout, otherwise reuse the existing
  // bounds to avoid scheduling a layout.

  int current_height = 0;
  if (*view) {
    current_height = (*view)->height();
    root_view_.RemoveChildView(*view);
  }

  int new_height = 0;
  if (new_view) {
    CSize preferred_size;
    new_view->GetPreferredSize(&preferred_size);
    new_height = preferred_size.cy;
    root_view_.AddChildView(new_view);
  }

  bool changed = false;
  if (new_height != current_height) {
    changed = true;
  } else if (new_view && *view) {
    // The view changed, but the new view wants the same size, give it the
    // bounds of the last view and have it repaint.
    new_view->SetBounds((*view)->bounds().ToRECT());
    new_view->SchedulePaint();
  } else if (new_view) {
    DCHECK(new_height == 0);
    // The heights are the same, but the old view is null. This only happens
    // when the height is zero. Zero out the bounds.
    new_view->SetBounds(0, 0, 0, 0);
  }
  *view = new_view;
  return changed;
}

void VistaFrame::SetWindowTitle(const std::wstring& title) {
  SetWindowText(title.c_str());
}

void VistaFrame::Activate() {
  if (IsIconic())
    ShowWindow(SW_RESTORE);
  MoveToFront(true);
}

void VistaFrame::FlashFrame() {
  FLASHWINFO flash_window_info;
  flash_window_info.cbSize = sizeof(FLASHWINFO);
  flash_window_info.hwnd = GetHWND();
  flash_window_info.dwFlags = FLASHW_ALL;
  flash_window_info.uCount = 4;
  flash_window_info.dwTimeout = 0;
  ::FlashWindowEx(&flash_window_info);
}

void VistaFrame::ShowTabContents(TabContents* selected_contents) {
  tab_contents_container_->SetTabContents(selected_contents);

  // Force a LoadingStateChanged notification because the TabContents
  // could be loading (such as when the user unconstrains a tab.
  if (selected_contents && selected_contents->delegate())
    selected_contents->delegate()->LoadingStateChanged(selected_contents);

  ShelfVisibilityChangedImpl(selected_contents);
}

TabStrip* VistaFrame::GetTabStrip() const {
  return tabstrip_;
}

void VistaFrame::ContinueDetachConstrainedWindowDrag(const gfx::Point& mouse_pt,
                                                     int frame_component) {
  // Need to force a paint at this point so that the newly created window looks
  // correct. (Otherwise parts of the tabstrip are clipped).
  CRect cr;
  GetClientRect(&cr);
  PaintNow(cr);

  // The user's mouse is already moving, and the left button is down, but we
  // need to start moving this frame, so we _post_ it a NCLBUTTONDOWN message
  // with the HTCAPTION flag to trick windows into believing the user just
  // started dragging on the title bar. All the frame moving is then handled
  // automatically by windows. Note that we use PostMessage here since we need
  // to return to the message loop first otherwise Windows' built in move code
  // will not be able to be triggered.
  POINTS pts;
  pts.x = mouse_pt.x();
  pts.y = mouse_pt.y();
  PostMessage(WM_NCLBUTTONDOWN, frame_component,
              reinterpret_cast<LPARAM>(&pts));
}

void VistaFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  // First we need to ensure everything has an initial size. Currently, the
  // window has the wrong size, but that's OK, doing this will allow us to
  // figure out how big all the UI bits are.
  Layout();

  // Now figure out the height of the non-client area (the Native windows
  // chrome) by comparing the window bounds to the client bounds.
  CRect window_bounds, client_bounds;
  GetBounds(&window_bounds, true);
  GetBounds(&client_bounds, false);
  int left_edge_width = client_bounds.left - window_bounds.left;
  int top_edge_height = client_bounds.top - window_bounds.top;
  int right_edge_width = window_bounds.right - client_bounds.right;
  int bottom_edge_height = window_bounds.bottom - client_bounds.bottom;

  // Now resize the window. This will result in Layout() getting called again
  // and the contents getting sized to the value specified in |contents_bounds|
  SetWindowPos(NULL, contents_bounds.x() - left_edge_width,
               contents_bounds.y() - top_edge_height,
               contents_bounds.width() + left_edge_width + right_edge_width,
               contents_bounds.height() + top_edge_height + bottom_edge_height,
               SWP_NOZORDER | SWP_NOACTIVATE);
}

void VistaFrame::SetIsOffTheRecord(bool value) {
  is_off_the_record_ = value;
}

TabContentsContainerView* VistaFrame::GetTabContentsContainer() {
  return tab_contents_container_;
}

void VistaFrame::DestroyBrowser() {
  // TODO(beng): (Cleanup) tidy this up, just fixing the build red for now.
  // Need to do this first, before the browser_ is deleted and we can't remove
  // the tabstrip from the model's observer list because the model was
  // destroyed with browser_.
  if (browser_) {
    if (bookmark_bar_view_.get() && bookmark_bar_view_->GetParent()) {
      // The bookmark bar should not be parented by the time we get here.
      // If you hit this NOTREACHED file a bug with the trace.
      NOTREACHED();
      bookmark_bar_view_->GetParent()->RemoveChildView(bookmark_bar_view_.get());
    }

    // Explicitly delete the BookmarkBarView now. That way we don't have to
    // worry about the BookmarkBarView potentially outliving the Browser &
    // Profile.
    bookmark_bar_view_.reset(NULL);

    browser_->tabstrip_model()->RemoveObserver(tabstrip_);
    delete browser_;
    browser_ = NULL;
  }
}

void VistaFrame::ShelfVisibilityChangedImpl(TabContents* current_tab) {
  // Coalesce layouts.
  bool changed = false;

  ChromeViews::View* new_shelf = NULL;
  if (current_tab && current_tab->IsDownloadShelfVisible())
    new_shelf = current_tab->GetDownloadShelfView();
  changed |= UpdateChildViewAndLayout(new_shelf, &shelf_view_);

  ChromeViews::View* new_info_bar = NULL;
  WebContents* web_contents = current_tab ? current_tab->AsWebContents() : NULL;
  if (web_contents && web_contents->view()->IsInfoBarVisible())
    new_info_bar = web_contents->view()->GetInfoBarView();
  changed |= UpdateChildViewAndLayout(new_info_bar, &info_bar_view_);

  ChromeViews::View* new_bookmark_bar_view = NULL;
  if (SupportsBookmarkBar())
    new_bookmark_bar_view = GetBookmarkBarView();
  changed |= UpdateChildViewAndLayout(new_bookmark_bar_view,
                                      &active_bookmark_bar_);

  // Only do a layout if the current contents is non-null. We assume that if the
  // contents is NULL, we're either being destroyed, or ShowTabContents is going
  // to be invoked with a non-null TabContents again so that there is no need
  // in doing a layout now (and would result in extra work/invalidation on
  // tab switches).
  if (changed && current_tab)
    Layout();
}
