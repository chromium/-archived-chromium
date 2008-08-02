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

#include "chrome/browser/xp_frame.h"

#include <windows.h>

#include "base/gfx/native_theme.h"
#include "base/gfx/rect.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/frame_util.h"
#include "chrome/browser/point_buffer.h"
#include "chrome/browser/suspend_controller.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_container_view.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/window_clipping_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/accessibility/view_accessibility.h"
#include "chrome/views/accelerator.h"
#include "chrome/views/background.h"
#include "chrome/views/event.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/views/hwnd_notification_source.h"
#include "chrome/views/tooltip_manager.h"
#include "chrome/views/view.h"

#include "generated_resources.h"

// Needed for accessibility support.
// NOTE(klink): To comply with a directive from Darin, I have added
// this #pragma directive instead of adding "oleacc.lib" to a project file.
#pragma comment(lib, "oleacc.lib")

// Layout constants and image size dependent values
static const int kZoomedTopMargin = 1;
static const int kZoomedBottomMargin = 1;

static const int kTopMargin = 16;
static const int kContentBorderHorizOffset = 2;
static const int kContentBorderVertTopOffset = 37;
static const int kContentBorderVertBottomOffset = 2;
static const int kToolbarOverlapVertOffset = 3;
static const int kTabShadowSize = 2;

static const int kDistributorLogoHorizontalOffset = 7;
static const int kDistributorLogoVerticalOffset = 3;

// Size of a corner. We use this when drawing a black background in maximized
// mode
static const int kCornerSize = 4;

// The visual size of the curved window corners - used when masking out the
// corners when resizing. This should vary as the shape of the curve varies
// in OnSize.
static const int kCurvedCornerSize = 3;

static int g_title_bar_height;
static int g_bottom_margin;
static int g_left_margin;
static int g_right_margin;

static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowControlsTopOffset = 1;
static const int kWindowControlsRightOffset = 4;
static const int kWindowControlsTopZoomedOffset = 1;
static const int kWindowControlsRightZoomedOffset = 3;

// Number of pixels still visible when the toolbar is invisible.
static const int kCollapsedToolbarHeight = 4;

// Minimum title bar height used when the tab strip is not visible.
static const int kMinTitleBarHeight = 25;

// OTR image offsets.
static const int kOTRImageHorizMargin = 2;
static const int kOTRImageVertMargin = 2;

// The line drawn to separate tab end contents.
static const int kSeparationLineHeight = 1;
static const SkColor kSeparationLineColor = SkColorSetRGB(178, 178, 178);

// Padding between the tab strip and the window controls in maximized mode.
static const int kZoomedStripPadding = 16;

using ChromeViews::Accelerator;
using ChromeViews::FocusManager;

////////////////////////////////////////////////////////////////////////////////
//
// RegionsUnderInfo class
//
// A facility to enumerate the windows obscured by a window. For each window,
// a region is provided.
//
////////////////////////////////////////////////////////////////////////////////
class RegionsUnderInfo {
 public:
  explicit RegionsUnderInfo(HWND hwnd) : hwnd_(hwnd),
                                found_hwnd_(false) {
    CRect window_rect;
    ::GetWindowRect(hwnd, &window_rect);
    hwnd_rgn_ = ::CreateRectRgn(window_rect.left, window_rect.top,
                                window_rect.right, window_rect.bottom);
    Init();
  }

  ~RegionsUnderInfo() {
    int i, region_count;
    region_count = static_cast<int>(regions_.size());
    for (i = 0; i < region_count; i++) {
      ::DeleteObject(regions_[i]);
    }

    ::DeleteObject(hwnd_rgn_);
  }

  int GetWindowCount() {
    return static_cast<int>(windows_.size());
  }

  HWND GetWindowAt(int index) {
    return windows_[index];
  }

  HRGN GetRegionAt(int index) {
    return regions_[index];
  }

 private:
  static BOOL CALLBACK WindowEnumProc(HWND hwnd, LPARAM lParam) {
    RegionsUnderInfo* rui =
        reinterpret_cast<RegionsUnderInfo*>(lParam);

    if (hwnd == rui->hwnd_) {
      rui->found_hwnd_ = true;
      return TRUE;
    }

    int status = ERROR;
    bool should_delete_rgn = true;
    if (rui->found_hwnd_) {
      if (::IsWindowVisible(hwnd)) {
        RECT r;
        ::GetWindowRect(hwnd, &r);
        HRGN tmp = ::CreateRectRgn(r.left, r.top, r.right, r.bottom);
        if (::CombineRgn(tmp,
                         rui->hwnd_rgn_,
                         tmp,
                         RGN_AND) != NULLREGION) {
          // Remove that intersection to exclude any window below
          int status = ::CombineRgn(rui->hwnd_rgn_,
                                    rui->hwnd_rgn_,
                                    tmp,
                                    RGN_DIFF);

          // We have an interesction, add it with the region in hwnd
          // coordinate system
          ::OffsetRgn(tmp, -r.left, -r.top);
          rui->windows_.push_back(hwnd);
          rui->regions_.push_back(tmp);
          should_delete_rgn = false;
        }
        if (should_delete_rgn) {
          ::DeleteObject(tmp);
        }
      }
    }

    // If hwnd_rgn_ is NULL, we are done
    if (status == NULLREGION) {
      return FALSE;
    } else {
      return TRUE;
    }
  }

  void Init() {
    ::EnumWindows(WindowEnumProc, reinterpret_cast<LPARAM>(this));
  }


  HWND hwnd_;
  HRGN hwnd_rgn_;
  bool found_hwnd_;
  std::vector<HWND> windows_;
  std::vector<HRGN> regions_;
};

////////////////////////////////////////////////////////////////////////////////
//
// XPFrame implementation
//
////////////////////////////////////////////////////////////////////////////////

// static
bool XPFrame::g_initialized = FALSE;

// static
HCURSOR XPFrame::g_resize_cursors[4];

// static
SkBitmap** XPFrame::g_bitmaps;

// static
SkBitmap** XPFrame::g_otr_bitmaps;

// Possible frame action.
enum FrameActionTag {
  MINIATURIZE_TAG = 0,
  MAXIMIZE_TAG,
  RESTORE_TAG,
  CLOSE_TAG,
};

static const int kImageNames[] = { IDR_WINDOW_BOTTOM_CENTER,
                                   IDR_WINDOW_BOTTOM_LEFT_CORNER,
                                   IDR_WINDOW_BOTTOM_RIGHT_CORNER,
                                   IDR_WINDOW_LEFT_SIDE,
                                   IDR_WINDOW_RIGHT_SIDE,
                                   IDR_WINDOW_TOP_CENTER,
                                   IDR_WINDOW_TOP_LEFT_CORNER,
                                   IDR_WINDOW_TOP_RIGHT_CORNER,
                                   IDR_CONTENT_BOTTOM_CENTER,
                                   IDR_CONTENT_BOTTOM_LEFT_CORNER,
                                   IDR_CONTENT_BOTTOM_RIGHT_CORNER,
                                   IDR_CONTENT_LEFT_SIDE,
                                   IDR_CONTENT_RIGHT_SIDE,
                                   IDR_CONTENT_TOP_CENTER,
                                   IDR_CONTENT_TOP_LEFT_CORNER,
                                   IDR_CONTENT_TOP_RIGHT_CORNER,
                                   IDR_DEWINDOW_BOTTOM_CENTER,
                                   IDR_DEWINDOW_BOTTOM_LEFT_CORNER,
                                   IDR_DEWINDOW_BOTTOM_RIGHT_CORNER,
                                   IDR_DEWINDOW_LEFT_SIDE,
                                   IDR_DEWINDOW_RIGHT_SIDE,
                                   IDR_DEWINDOW_TOP_CENTER,
                                   IDR_DEWINDOW_TOP_LEFT_CORNER,
                                   IDR_DEWINDOW_TOP_RIGHT_CORNER,
                                   IDR_APP_TOP_LEFT,
                                   IDR_APP_TOP_CENTER,
                                   IDR_APP_TOP_RIGHT
};

static const int kOTRImageNames[] = { IDR_WINDOW_BOTTOM_CENTER_OTR,
                                      IDR_WINDOW_BOTTOM_LEFT_CORNER_OTR,
                                      IDR_WINDOW_BOTTOM_RIGHT_CORNER_OTR,
                                      IDR_WINDOW_LEFT_SIDE_OTR,
                                      IDR_WINDOW_RIGHT_SIDE_OTR,
                                      IDR_WINDOW_TOP_CENTER_OTR,
                                      IDR_WINDOW_TOP_LEFT_CORNER_OTR,
                                      IDR_WINDOW_TOP_RIGHT_CORNER_OTR,
                                      IDR_CONTENT_BOTTOM_CENTER,
                                      IDR_CONTENT_BOTTOM_LEFT_CORNER,
                                      IDR_CONTENT_BOTTOM_RIGHT_CORNER,
                                      IDR_CONTENT_LEFT_SIDE,
                                      IDR_CONTENT_RIGHT_SIDE,
                                      IDR_CONTENT_TOP_CENTER,
                                      IDR_CONTENT_TOP_LEFT_CORNER,
                                      IDR_CONTENT_TOP_RIGHT_CORNER,
                                      IDR_DEWINDOW_BOTTOM_CENTER_OTR,
                                      IDR_DEWINDOW_BOTTOM_LEFT_CORNER_OTR,
                                      IDR_DEWINDOW_BOTTOM_RIGHT_CORNER_OTR,
                                      IDR_DEWINDOW_LEFT_SIDE_OTR,
                                      IDR_DEWINDOW_RIGHT_SIDE_OTR,
                                      IDR_DEWINDOW_TOP_CENTER_OTR,
                                      IDR_DEWINDOW_TOP_LEFT_CORNER_OTR,
                                      IDR_DEWINDOW_TOP_RIGHT_CORNER_OTR,
                                      IDR_APP_TOP_LEFT,
                                      IDR_APP_TOP_CENTER,
                                      IDR_APP_TOP_RIGHT
};

typedef enum { BOTTOM_CENTER = 0, BOTTOM_LEFT_CORNER, BOTTOM_RIGHT_CORNER,
               LEFT_SIDE, RIGHT_SIDE, TOP_CENTER, TOP_LEFT_CORNER,
               TOP_RIGHT_CORNER, CT_BOTTOM_CENTER,
               CT_BOTTOM_LEFT_CORNER, CT_BOTTOM_RIGHT_CORNER, CT_LEFT_SIDE,
               CT_RIGHT_SIDE, CT_TOP_CENTER, CT_TOP_LEFT_CORNER,
               CT_TOP_RIGHT_CORNER, DE_BOTTOM_CENTER,
               DE_BOTTOM_LEFT_CORNER, DE_BOTTOM_RIGHT_CORNER, DE_LEFT_SIDE,
               DE_RIGHT_SIDE, DE_TOP_CENTER, DE_TOP_LEFT_CORNER,
               DE_TOP_RIGHT_CORNER,
               APP_TOP_LEFT, APP_TOP_CENTER, APP_TOP_RIGHT
};

// static
XPFrame* XPFrame::CreateFrame(const gfx::Rect& bounds,
                              Browser* browser,
                              bool is_otr) {
  XPFrame* instance = new XPFrame(browser);
  instance->Create(NULL, bounds.ToRECT(),
      l10n_util::GetString(IDS_PRODUCT_NAME).c_str());
  instance->InitAfterHWNDCreated();
  instance->SetIsOffTheRecord(is_otr);
  FocusManager::CreateFocusManager(instance->m_hWnd, &(instance->root_view_));
  return instance;
}

XPFrame::XPFrame(Browser* browser)
    : browser_(browser),
      root_view_(this, true),
      frame_view_(NULL),
      tabstrip_(NULL),
      active_bookmark_bar_(NULL),
      tab_contents_container_(NULL),
      min_button_(NULL),
      max_button_(NULL),
      restore_button_(NULL),
      close_button_(NULL),
      should_save_window_placement_(browser->GetType() != BrowserType::BROWSER),
      saved_window_placement_(false),
      current_action_(FA_NONE),
      on_mouse_leave_armed_(false),
      browser_paint_pending_(false),
      previous_cursor_(NULL),
      minimum_size_(100, 100),
      shelf_view_(NULL),
      bookmark_bar_view_(NULL),
      info_bar_view_(NULL),
      is_active_(false),
      is_off_the_record_(false),
      title_bar_height_(0),
      off_the_record_image_(NULL),
      distributor_logo_(NULL),
      ignore_ncactivate_(false),
      paint_as_active_(false),
      browser_view_(NULL) {
  InitializeIfNeeded();
}

XPFrame::~XPFrame() {
  DestroyBrowser();
}

void XPFrame::InitAfterHWNDCreated() {
  tooltip_manager_.reset(new ChromeViews::TooltipManager(this, m_hWnd));
}

ChromeViews::TooltipManager* XPFrame::GetTooltipManager() {
  return tooltip_manager_.get();
}

StatusBubble* XPFrame::GetStatusBubble() {
  return NULL;
}

void XPFrame::InitializeIfNeeded() {
  if (!g_initialized) {
    g_resize_cursors[RC_VERTICAL]   = LoadCursor(NULL, IDC_SIZENS);
    g_resize_cursors[RC_HORIZONTAL] = LoadCursor(NULL, IDC_SIZEWE);
    g_resize_cursors[RC_NESW]       = LoadCursor(NULL, IDC_SIZENESW);
    g_resize_cursors[RC_NWSE]       = LoadCursor(NULL, IDC_SIZENWSE);

    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    g_bitmaps = new SkBitmap*[arraysize(kImageNames)];
    g_otr_bitmaps = new SkBitmap*[arraysize(kOTRImageNames)];
    for (int i = 0; i < arraysize(kImageNames); i++) {
      g_bitmaps[i] = rb.GetBitmapNamed(kImageNames[i]);
      g_otr_bitmaps[i] = rb.GetBitmapNamed(kOTRImageNames[i]);
    }

    g_bottom_margin = (int) kContentBorderVertBottomOffset +
        g_bitmaps[CT_BOTTOM_CENTER]->height();
    g_left_margin = (int) kContentBorderHorizOffset +
        g_bitmaps[CT_LEFT_SIDE]->width();
    g_right_margin = g_left_margin;
    g_title_bar_height = (int) kContentBorderVertTopOffset +
        g_bitmaps[CT_TOP_CENTER]->height();

    g_initialized = TRUE;
  }
}

void XPFrame::Init() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  FrameUtil::RegisterBrowserWindow(this);

  // Link the HWND with its root view so we can retrieve the RootView from the
  // HWND for automation purposes.
  ChromeViews::SetRootViewForHWND(m_hWnd, &root_view_);

  // Remove the WS_CAPTION explicitely because we don't want a window style
  // title bar
  SetWindowLongPtr(GWL_STYLE,
                   (GetWindowLongPtr(GWL_STYLE) & ~WS_CAPTION) | WS_BORDER);

  frame_view_ = new XPFrameView(this);
  root_view_.AddChildView(frame_view_);
  root_view_.SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  frame_view_->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  // Use a white background. This will be the color of the content area until
  // the first tab has started, so we want it to look minimally jarring when
  // it is replaced by web content.
  //
  // TODO(brettw) if we have a preference for default page background, this
  // color should be the same.
  root_view_.SetBackground(
      ChromeViews::Background::CreateSolidBackground(SK_ColorWHITE));

  browser_view_ = new BrowserView(this, browser_, NULL, NULL);
  frame_view_->AddChildView(browser_view_);

  tabstrip_ = CreateTabStrip(browser_);
  tabstrip_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  frame_view_->AddChildView(tabstrip_);

  tab_contents_container_ = new TabContentsContainerView();
  frame_view_->AddChildView(tab_contents_container_);

  if (is_off_the_record_) {
    off_the_record_image_ = new ChromeViews::ImageView();
    SkBitmap* otr_icon = rb.GetBitmapNamed(IDR_OTR_ICON);
    off_the_record_image_->SetImage(*otr_icon);
    off_the_record_image_->SetTooltipText(
        l10n_util::GetString(IDS_OFF_THE_RECORD_TOOLTIP));
    frame_view_->AddChildView(off_the_record_image_);
    frame_view_->AddViewToDropList(off_the_record_image_);
  }

  distributor_logo_ = new ChromeViews::ImageView();
  frame_view_->AddViewToDropList(distributor_logo_);
  distributor_logo_->SetImage(rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO_LIGHT));
  frame_view_->AddChildView(distributor_logo_);

  min_button_ = new ChromeViews::Button();
  min_button_->SetListener(this, MINIATURIZE_TAG);
  min_button_->SetImage(ChromeViews::Button::BS_NORMAL,
                        rb.GetBitmapNamed(IDR_MINIMIZE));
  min_button_->SetImage(ChromeViews::Button::BS_HOT,
                        rb.GetBitmapNamed(IDR_MINIMIZE_H));
  min_button_->SetImage(ChromeViews::Button::BS_PUSHED,
                        rb.GetBitmapNamed(IDR_MINIMIZE_P));
  min_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_MINIMIZE));
  min_button_->SetTooltipText(
      l10n_util::GetString(IDS_XPFRAME_MINIMIZE_TOOLTIP));
  frame_view_->AddChildView(min_button_);

  max_button_ = new ChromeViews::Button();
  max_button_->SetListener(this, MAXIMIZE_TAG);
  max_button_->SetImage(ChromeViews::Button::BS_NORMAL,
                        rb.GetBitmapNamed(IDR_MAXIMIZE));
  max_button_->SetImage(ChromeViews::Button::BS_HOT,
                        rb.GetBitmapNamed(IDR_MAXIMIZE_H));
  max_button_->SetImage(ChromeViews::Button::BS_PUSHED,
                        rb.GetBitmapNamed(IDR_MAXIMIZE_P));
  max_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_MAXIMIZE));
  max_button_->SetTooltipText(
      l10n_util::GetString(IDS_XPFRAME_MAXIMIZE_TOOLTIP));
  frame_view_->AddChildView(max_button_);

  restore_button_ = new ChromeViews::Button();
  restore_button_->SetListener(this, RESTORE_TAG);
  restore_button_->SetImage(ChromeViews::Button::BS_NORMAL,
                            rb.GetBitmapNamed(IDR_RESTORE));
  restore_button_->SetImage(ChromeViews::Button::BS_HOT,
                            rb.GetBitmapNamed(IDR_RESTORE_H));
  restore_button_->SetImage(ChromeViews::Button::BS_PUSHED,
                            rb.GetBitmapNamed(IDR_RESTORE_P));
  restore_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RESTORE));
  restore_button_->SetTooltipText(
      l10n_util::GetString(IDS_XPFRAME_RESTORE_TOOLTIP));
  frame_view_->AddChildView(restore_button_);

  close_button_ = new ChromeViews::Button();
  close_button_->SetListener(this, CLOSE_TAG);
  close_button_->SetImage(ChromeViews::Button::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE));
  close_button_->SetImage(ChromeViews::Button::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_H));
  close_button_->SetImage(ChromeViews::Button::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_P));
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  close_button_->SetTooltipText(
      l10n_util::GetString(IDS_XPFRAME_CLOSE_TOOLTIP));
  frame_view_->AddChildView(close_button_);

  // Add the task manager item to the system menu before the last entry.
  task_manager_label_text_ = l10n_util::GetString(IDS_TASKMANAGER);
  HMENU system_menu = ::GetSystemMenu(m_hWnd, FALSE);
  int index = ::GetMenuItemCount(system_menu) - 1;
  if (index < 0) {
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
}

TabStrip* XPFrame::CreateTabStrip(Browser* browser) {
  return new TabStrip(browser->tabstrip_model());
}

void XPFrame::Show(int command, bool adjust_to_fit) {
  if (adjust_to_fit)
    win_util::AdjustWindowToFit(*this);
  ::ShowWindow(*this, command);
}

void* XPFrame::GetPlatformID() {
  return reinterpret_cast<void*>(m_hWnd);
}

int XPFrame::GetContentsYOrigin() {
  int min_y = tab_contents_container_->GetY();
  if (info_bar_view_)
    min_y = std::min(min_y, info_bar_view_->GetY());

  if (bookmark_bar_view_.get())
    min_y = std::min(min_y, bookmark_bar_view_->GetY());

  return min_y;
}

SkBitmap** XPFrame::GetFrameBitmaps() {
  if (is_off_the_record_)
    return g_otr_bitmaps;
  else
    return g_bitmaps;
}

void XPFrame::Layout() {
  CRect client_rect;
  GetClientRect(&client_rect);
  int width = client_rect.Width();
  int height = client_rect.Height();

  root_view_.SetBounds(0, 0, width, height);
  frame_view_->SetBounds(0, 0, width, height);

  CSize preferred_size;

  if (IsZoomed()) {
    close_button_->GetPreferredSize(&preferred_size);
    close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                     ChromeViews::Button::ALIGN_BOTTOM);
    close_button_->SetBounds(width - preferred_size.cx -
                             kWindowControlsRightZoomedOffset,
                             0,
                             preferred_size.cx +
                             kWindowControlsRightZoomedOffset,
                             preferred_size.cy +
                             kWindowControlsTopZoomedOffset);

    max_button_->SetVisible(false);

    restore_button_->SetVisible(true);
    restore_button_->GetPreferredSize(&preferred_size);
    restore_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                       ChromeViews::Button::ALIGN_BOTTOM);
    restore_button_->SetBounds(close_button_->GetX() - preferred_size.cx,
                               0,
                               preferred_size.cx,
                               preferred_size.cy +
                               kWindowControlsTopZoomedOffset);

    min_button_->GetPreferredSize(&preferred_size);
    min_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                   ChromeViews::Button::ALIGN_BOTTOM);
    min_button_->SetBounds(restore_button_->GetX() - preferred_size.cx,
                           0,
                           preferred_size.cx,
                           preferred_size.cy +
                           kWindowControlsTopZoomedOffset);

  } else {
    close_button_->GetPreferredSize(&preferred_size);
    close_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                     ChromeViews::Button::ALIGN_TOP);
    close_button_->SetBounds(width - kWindowControlsRightOffset -
                             preferred_size.cx,
                             kWindowControlsTopOffset,
                             preferred_size.cx,
                             preferred_size.cy);

    restore_button_->SetVisible(false);

    max_button_->SetVisible(true);
    max_button_->GetPreferredSize(&preferred_size);
    max_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                   ChromeViews::Button::ALIGN_TOP);
    max_button_->SetBounds(close_button_->GetX() - preferred_size.cx,
                           kWindowControlsTopOffset,
                           preferred_size.cx,
                           preferred_size.cy);

    min_button_->GetPreferredSize(&preferred_size);
    min_button_->SetImageAlignment(ChromeViews::Button::ALIGN_LEFT,
                                   ChromeViews::Button::ALIGN_TOP);
    min_button_->SetBounds(max_button_->GetX() - preferred_size.cx,
                           kWindowControlsTopOffset,
                           preferred_size.cx,
                           preferred_size.cy);
  }

  int right_limit = min_button_->GetX();
  int left_margin;
  int right_margin;
  int bottom_margin;
  int top_margin;

  SkBitmap** bitmaps = GetFrameBitmaps();
  if (IsZoomed()) {
    right_limit -= kZoomedStripPadding;
    top_margin = kZoomedTopMargin;
    bottom_margin = kZoomedBottomMargin;
    left_margin = 0;
    right_margin = 0;
  } else {
    top_margin = kTopMargin;
    bottom_margin = g_bottom_margin;
    left_margin = g_left_margin;
    right_margin = g_right_margin;
  }

  int last_y = top_margin - 1;
  if (IsTabStripVisible()) {
    int tab_strip_x = left_margin;

    if (is_off_the_record_) {
      CSize otr_image_size;
      off_the_record_image_->GetPreferredSize(&otr_image_size);
      tab_strip_x += otr_image_size.cx + (2 * kOTRImageHorizMargin);
      if (IsZoomed()) {
        off_the_record_image_->SetBounds(left_margin + kOTRImageHorizMargin,
                                         top_margin + 1,
                                         otr_image_size.cx,
                                         tabstrip_->GetPreferredHeight() -
                                         kToolbarOverlapVertOffset - 1);
      } else {
        off_the_record_image_->SetBounds(left_margin + kOTRImageHorizMargin,
                                         top_margin - 1 +
                                         tabstrip_->GetPreferredHeight() -
                                         otr_image_size.cy -
                                         kOTRImageVertMargin,
                                         otr_image_size.cx,
                                         otr_image_size.cy);
      }
    }

    if (IsZoomed()) {
      distributor_logo_->SetVisible(false);
    } else {
      CSize distributor_logo_size;
      distributor_logo_->GetPreferredSize(&distributor_logo_size);
      distributor_logo_->SetVisible(true);
      distributor_logo_->SetBounds(min_button_->GetX() - 
                                       distributor_logo_size.cx -
                                       kDistributorLogoHorizontalOffset,
                                   kDistributorLogoVerticalOffset,
                                   distributor_logo_size.cx,
                                   distributor_logo_size.cy);
    }

    tabstrip_->SetBounds(tab_strip_x, top_margin - 1,
                         right_limit - tab_strip_x - right_margin,
                         tabstrip_->GetPreferredHeight());

    last_y = tabstrip_->GetY() + tabstrip_->GetHeight();
  } else {
    tabstrip_->SetBounds(0, 0, 0, 0);
    tabstrip_->SetVisible(false);
    if (is_off_the_record_)
      off_the_record_image_->SetVisible(false);
  }

  if (IsToolBarVisible()) {
    browser_view_->SetVisible(true);
    browser_view_->SetBounds(left_margin,
                             last_y - kToolbarOverlapVertOffset,
                             width - left_margin - right_margin,
                             bitmaps[CT_TOP_CENTER]->height());
    browser_view_->Layout();
    title_bar_height_ = browser_view_->GetY();
    last_y = browser_view_->GetY() + browser_view_->GetHeight();
  } else {
    // If the tab strip is visible, we need to expose the toolbar for a small
    // offset. (kCollapsedToolbarHeight).
    if (IsTabStripVisible()) {
      title_bar_height_ = last_y;
      last_y += kCollapsedToolbarHeight;
    } else {
      last_y = std::max(kMinTitleBarHeight,
                        close_button_->GetY() + close_button_->GetHeight());
      title_bar_height_ = last_y;
    }
    browser_view_->SetVisible(false);
  }

  int browser_h = height - last_y - bottom_margin;
  if (shelf_view_) {
    shelf_view_->GetPreferredSize(&preferred_size);
    shelf_view_->SetBounds(left_margin,
                           height - bottom_margin - preferred_size.cy,
                           width - left_margin - right_margin,
                           preferred_size.cy);
    browser_h -= preferred_size.cy;
  }

  int bookmark_bar_height = 0;

  CSize bookmark_bar_size;
  CSize info_bar_size;

  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->GetPreferredSize(&bookmark_bar_size);
    bookmark_bar_height = bookmark_bar_size.cy;
  }

  if (info_bar_view_)
    info_bar_view_->GetPreferredSize(&info_bar_size);

  // If we're showing a bookmarks bar in the new tab page style and we
  // have an infobar showing, we need to flip them.
  if (info_bar_view_ &&
      bookmark_bar_view_.get() &&
      bookmark_bar_view_->IsNewTabPage() &&
      !bookmark_bar_view_->IsAlwaysShown()) {
    info_bar_view_->SetBounds(left_margin,
                              last_y,
                              client_rect.Width() - left_margin - right_margin,
                              info_bar_size.cy);
    browser_h -= info_bar_size.cy;
    last_y += info_bar_size.cy;

    last_y -= kSeparationLineHeight;

    bookmark_bar_view_->SetBounds(left_margin,
                                  last_y,
                                  client_rect.Width() - left_margin -
                                  right_margin,
                                  bookmark_bar_size.cy);
    browser_h -= (bookmark_bar_size.cy - kSeparationLineHeight);
    last_y += bookmark_bar_size.cy;
  } else {
    if (bookmark_bar_view_.get()) {
      // We want our bookmarks bar to be responsible for drawing its own
      // separator, so we let it overlap ours.
      last_y -= kSeparationLineHeight;

      bookmark_bar_view_->SetBounds(left_margin,
                                    last_y,
                                    client_rect.Width() - left_margin -
                                    right_margin,
                                    bookmark_bar_size.cy);
      browser_h -= (bookmark_bar_size.cy - kSeparationLineHeight);
      last_y += bookmark_bar_size.cy;
    }

    if (info_bar_view_) {
      info_bar_view_->SetBounds(left_margin,
                                last_y,
                                client_rect.Width() -
                                    left_margin - right_margin,
                                info_bar_size.cy);
      browser_h -= info_bar_size.cy;
      last_y += info_bar_size.cy;
    }
  }

  tab_contents_container_->SetBounds(left_margin,
                                     last_y,
                                     width - left_margin - right_margin,
                                     browser_h);

  browser_view_->LayoutStatusBubble(last_y + browser_h);

  frame_view_->SchedulePaint();
}

// This is called when we receive WM_ENDSESSION. We have 5 seconds to quit
// the application or we are going to be flagged as flaky.
void XPFrame::OnEndSession(BOOL ending, UINT logoff) {
  tabstrip_->AbortActiveDragSession();
  FrameUtil::EndSession();
}

// Note: called directly by the handler macros to handle WM_CLOSE messages.
void XPFrame::Close() {
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
    if (current_action_ == FA_RESIZING)
      StopWindowResize();
    // NOTE: Don't use ShowWindow(SW_HIDE) here, otherwise end session blocks
    // here.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
    browser_->OnWindowClosing();
  } else {
    // Empty tab strip, it's now safe to do the final clean-up.

    root_view_.OnViewContainerDestroyed();

    NotificationService::current()->Notify(
        NOTIFY_WINDOW_CLOSED, Source<HWND>(m_hWnd),
        NotificationService::NoDetails());
    DestroyWindow();
  }
}

void XPFrame::OnFinalMessage(HWND hwnd) {
  delete this;
}

void XPFrame::SetAcceleratorTable(
    std::map<Accelerator, int>* accelerator_table) {
  accelerator_table_.reset(accelerator_table);
}

bool XPFrame::GetAccelerator(int cmd_id, ChromeViews::Accelerator* accelerator)
  {
  std::map<ChromeViews::Accelerator, int>::iterator it =
      accelerator_table_->begin();
  for (; it != accelerator_table_->end(); ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}


bool XPFrame::AcceleratorPressed(const Accelerator& accelerator) {
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

////////////////////////////////////////////////////////////////////////////////
//
// Events
//
////////////////////////////////////////////////////////////////////////////////
LRESULT XPFrame::OnSettingChange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                                 BOOL& handled) {
  // Set this to false, if we actually handle the message, we'll set it to
  // true below.
  handled = FALSE;
  if (w_param != SPI_SETWORKAREA)
    return 0;  // Return value is effectively ignored in atlwin.h.

  win_util::AdjustWindowToFit(m_hWnd);
  handled = TRUE;
  return 0;  // Return value is effectively ignored in atlwin.h.
}

LRESULT XPFrame::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // By default the client side is set to the window size which is what
  // we want.
  return 0;
}

LRESULT XPFrame::OnNCPaint(HRGN param) {
  return 0;
}

LRESULT XPFrame::OnMouseRange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                              BOOL& handled) {
  tooltip_manager_->OnMouse(u_msg, w_param, l_param);
  handled = FALSE;
  return 0;
}

LRESULT XPFrame::OnNotify(int w_param, NMHDR* l_param) {
  bool handled;
  LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
  SetMsgHandled(handled);
  return result;
}

void XPFrame::OnMove(const CSize& size) {
  if (!saved_window_placement_ && should_save_window_placement_)
    browser_->SaveWindowPlacementToDatabase();

  browser_->WindowMoved();
}

void XPFrame::OnMoving(UINT param, const LPRECT new_bounds) {
  // We want to let the browser know that the window moved so that it can
  // update the positions of any dependent WS_POPUPs
  browser_->WindowMoved();
}

void XPFrame::OnSize(UINT param, const CSize& size) {
  if (IsZoomed()) {
    SetWindowRgn(NULL, TRUE);
  } else if (IsIconic()) {
    // After minimizing the window, Windows will send us an OnSize
    // message where size is equal to the bounds of the entry in the
    // task bar. This is obviously bogus for our purposes and will
    // just confuse Layout() so bail.
    return;
  } else {
    PointBuffer o;

    // Redefine the window visible region for the new size
    o.AddPoint(0, 3);
    o.AddPoint(1, 1);
    o.AddPoint(3, 0);

    o.AddPoint(size.cx - 3, 0);
    o.AddPoint(size.cx - 1, 1);
    o.AddPoint(size.cx - 1, 3);
    o.AddPoint(size.cx - 0, 3);

    o.AddPoint(size.cx, size.cy);
    o.AddPoint(0, size.cy);

    // When resizing we don't want an extra paint to limit flicker
    SetWindowRgn(o.GetCurrentPolygonRegion(),
                 current_action_ == FA_RESIZING ? FALSE : TRUE);
  }

  // Layout our views
  Layout();

  // We paint immediately during a resize because it will feel laggy otherwise.
  if (root_view_.NeedsPainting(false)) {
    RedrawWindow(root_view_.GetScheduledPaintRect(),
                 NULL,
                 RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
    MessageLoop::current()->PumpOutPendingPaintMessages();
  }

  if (!saved_window_placement_ && should_save_window_placement_)
    browser_->SaveWindowPlacementToDatabase();
}

void XPFrame::OnNCLButtonDown(UINT flags, const CPoint& pt) {
  // DefWindowProc implementation for WM_NCLBUTTONDOWN will allow a
  // maximized window to move if the window size is less than screen
  // size. We have to handle this message to suppress this behavior.
  if (ShouldWorkAroundAutoHideTaskbar() && IsZoomed()) {
    if (GetForegroundWindow() != m_hWnd) {
      SetForegroundWindow(m_hWnd);
    }
  } else {
    SetMsgHandled(FALSE);
  }
}

void XPFrame::OnNCMButtonDown(UINT flags, const CPoint& pt) {
  // The point is in screen coordinate system so we need to convert
  CRect window_rect;
  GetWindowRect(&window_rect);
  CPoint point(pt);
  point.x -= window_rect.left;
  point.y -= window_rect.top;

  // Yes we need to add MK_MBUTTON. Windows doesn't include it
  OnMouseButtonDown(flags | MK_MBUTTON, point);
}

void XPFrame::OnNCRButtonDown(UINT flags, const CPoint& pt) {
  // The point is in screen coordinate system so we need to convert
  CRect window_rect;
  GetWindowRect(&window_rect);
  CPoint point(pt);
  point.x -= window_rect.left;
  point.y -= window_rect.top;

  // Yes we need to add MK_RBUTTON. Windows doesn't include it
  OnMouseButtonDown(flags | MK_RBUTTON, point);
}

void XPFrame::OnMouseButtonDown(UINT flags, const CPoint& pt) {
  using namespace ChromeViews;
  if (m_hWnd == NULL || !IsWindowVisible()) {
    return;
  }

  CRect original_rect;

  GetWindowRect(&original_rect);
  int width = original_rect.Width();
  int height= original_rect.Height();

  if (!ProcessMousePressed(pt, flags, FALSE)) {
    // Edge case when showing a menu that will close the window.
    if (!IsVisible())
      return;
    if (flags & MK_LBUTTON) {
      if (!IsZoomed()) {
        ResizeMode rm = ComputeResizeMode(pt.x, pt.y, width, height);
        if (rm != RM_UNDEFINED) {
          StartWindowResize(rm, pt);
        }
      }
    } else if (flags & MK_RBUTTON && pt.y < title_bar_height_) {
      CRect r;
      ::GetWindowRect(*this, &r);
      ShowSystemMenu(r.left + pt.x, r.top + pt.y);
    }
  }
}

void XPFrame::OnMouseButtonUp(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  if (flags & MK_LBUTTON) {
    switch (current_action_) {
      case FA_RESIZING:
        StopWindowResize();
        break;
      case FA_FORWARDING:
        ProcessMouseReleased(pt, flags);
        break;
    }
  } else {
    ProcessMouseReleased(pt, flags);
  }
}

void XPFrame::OnMouseButtonDblClk(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  if (!ProcessMousePressed(pt, flags, TRUE)) {
    CRect original_rect;
    GetWindowRect(&original_rect);
    int width = original_rect.Width();
    int height= original_rect.Height();

    // If the user double clicked on a resize area, ignore.
    if (ComputeResizeMode(pt.x, pt.y, width, height) == RM_UNDEFINED &&
        pt.y < title_bar_height_) {
      if (flags & MK_LBUTTON) {
        if (IsZoomed()) {
          ShowWindow(SW_RESTORE);
        } else {
          ShowWindow(SW_MAXIMIZE);
        }
      }
    }
  }
}

void XPFrame::OnLButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_LBUTTON, pt);
}

void XPFrame::OnMButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_MBUTTON, pt);
}

void XPFrame::OnRButtonUp(UINT flags, const CPoint& pt) {
  OnMouseButtonUp(flags | MK_RBUTTON, pt);
}

void XPFrame::OnMouseMove(UINT flags, const CPoint& pt) {
  if (m_hWnd == NULL) {
    return;
  }

  switch (current_action_) {
    case FA_NONE: {
      ArmOnMouseLeave();
      ProcessMouseMoved(pt, flags);
      if (!IsZoomed()) {
        CRect r;
        GetWindowRect(&r);
        XPFrame::ResizeMode rm = ComputeResizeMode(pt.x,
                                                   pt.y,
                                                   r.Width(),
                                                   r.Height());
        SetResizeCursor(rm);
      }
      break;
    }
    case FA_RESIZING: {
      ProcessWindowResize(pt);
      break;
    }
    case FA_FORWARDING: {
      ProcessMouseDragged(pt, flags);
      break;
    }
  }
}

void XPFrame::OnMouseLeave() {
  if (m_hWnd == NULL) {
    return;
  }
  ProcessMouseExited();
  on_mouse_leave_armed_ = FALSE;
}

LRESULT XPFrame::OnGetObject(UINT uMsg, WPARAM w_param, LPARAM object_id) {
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

void XPFrame::OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags) {
  ChromeViews::KeyEvent event(ChromeViews::Event::ET_KEY_PRESSED, c,
                              rep_cnt, flags);
  root_view_.ProcessKeyEvent(event);
}

void XPFrame::OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags) {
  ChromeViews::KeyEvent event(ChromeViews::Event::ET_KEY_RELEASED, c,
                              rep_cnt, flags);
  root_view_.ProcessKeyEvent(event);
}

void XPFrame::OnActivate(UINT n_state, BOOL is_minimized, HWND other) {
  if (FrameUtil::ActivateAppModalDialog(browser_))
    return;

  // We get deactivation notices before the window is deactivated,
  // so we need our Paint methods to know which type of window to
  // draw.
  is_active_ = (n_state != WA_INACTIVE);

  if (!is_minimized)  {
    browser_->WindowActivationChanged(is_active_);

    // Redraw the frame.
    frame_view_->SchedulePaint();

    // We need to force a paint now, as a user dragging a window
    // will block painting operations while the move is in progress.
    PaintNow(root_view_.GetScheduledPaintRect());
  }
}

int XPFrame::OnMouseActivate(CWindow wndTopLevel, UINT nHitTest, UINT message) {
  return FrameUtil::ActivateAppModalDialog(browser_) ? MA_NOACTIVATEANDEAT
                                                     : MA_ACTIVATE;
}

void XPFrame::OnPaint(HDC dc) {
  root_view_.OnPaint(*this);
}

LRESULT XPFrame::OnEraseBkgnd(HDC dc) {
  SetMsgHandled(TRUE);
  return 1;
}

void XPFrame::OnMinMaxInfo(LPMINMAXINFO mm_info) {
  // From MINMAXINFO documentation:
  // For systems with multiple monitors, the ptMaxSize and ptMaxPosition members
  // describe the maximized size and position of the window on the primary
  // monitor, even if the window ultimately maximizes onto a secondary monitor.
  // In that case, the window manager adjusts these values to compensate for
  // differences between the primary monitor and the monitor that displays the
  // window. Thus, if the user leaves ptMaxSize untouched, a window on a monitor
  // larger than the primary monitor maximizes to the size of the larger
  // monitor.
  //
  // But what the documentation doesn't say is that we need to compensate for
  // the taskbar. :/
  //
  // - So use the primary monitor for position and size calculation.
  // - Take in account the existence or not of the task bar in the destination
  //   monitor and adjust accordingly.
  // 99% of the time, they will contain mostly the same information.

  HMONITOR primary_monitor = ::MonitorFromWindow(NULL,
                                                 MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO primary_info;
  primary_info.cbSize = sizeof(primary_info);
  GetMonitorInfo(primary_monitor, &primary_info);

  HMONITOR destination_monitor = ::MonitorFromWindow(m_hWnd,
                                                     MONITOR_DEFAULTTONEAREST);
  MONITORINFO destination_info;
  destination_info.cbSize = sizeof(destination_info);
  GetMonitorInfo(destination_monitor, &destination_info);

  // Take in account the destination monitor taskbar location but the primary
  // monitor size.
  int primary_monitor_width =
      primary_info.rcMonitor.right - primary_info.rcMonitor.left;
  int primary_monitor_height =
      primary_info.rcMonitor.bottom - primary_info.rcMonitor.top;
  mm_info->ptMaxSize.x =
      primary_monitor_width -
      (destination_info.rcMonitor.right - destination_info.rcWork.right) -
      (destination_info.rcWork.left - destination_info.rcMonitor.left);
  mm_info->ptMaxSize.y =
      primary_monitor_height -
      (destination_info.rcMonitor.bottom - destination_info.rcWork.bottom) -
      (destination_info.rcWork.top - destination_info.rcMonitor.top);

  mm_info->ptMaxPosition.x = destination_info.rcWork.left -
                             destination_info.rcMonitor.left;
  mm_info->ptMaxPosition.y = destination_info.rcWork.top -
                             destination_info.rcMonitor.top;

  if (primary_monitor == destination_monitor) {
    // Only add support for auto-hidding taskbar on primary monitor.
  if (ShouldWorkAroundAutoHideTaskbar() &&
      EqualRect(&destination_info.rcWork, &destination_info.rcMonitor)) {
    mm_info->ptMaxSize.y--;
    }
  } else {
    // If the taskbar is on the second monitor, the difference in monitor size
    // won't be added. The reason: if the maximized size is less than the
    // primary monitor size, it won't get resized to the full screen of the
    // destination monitor (!) The position will get fixed in any case, just not
    // the size. The problem is that if we preemptly add the monitor size
    // difference, the window will get larger than the primary monitor size and
    // Windows will add (again) the monitor size difference!
    //
    // So for now, simply don't support the taskbar on a secondary monitor with
    // different monitor sizes.
#ifdef BUG_943445_FIXED
    if (mm_info->ptMaxSize.x < primary_monitor_width ||
        mm_info->ptMaxSize.y < primary_monitor_height) {
      int dst_monitor_width = destination_info.rcMonitor.right -
                              destination_info.rcMonitor.left;
      mm_info->ptMaxSize.x += (dst_monitor_width - primary_monitor_width);
      int dst_monitor_height = destination_info.rcMonitor.bottom -
                               destination_info.rcMonitor.top;
      mm_info->ptMaxSize.y += (dst_monitor_height - primary_monitor_height);
    }
#endif  // BUG_943445_FIXED
  }
  // If you fully understand what's going on, you can now appreciate the joyness
  // of programming on Windows.
}

void XPFrame::OnCaptureChanged(HWND hwnd) {
  if (current_action_ == FA_FORWARDING)
    root_view_.ProcessMouseDragCanceled();
  current_action_ = FA_NONE;
}

LRESULT XPFrame::OnNCHitTest(const CPoint& pt) {
  CPoint p(pt);
  CRect r;
  GetWindowRect(&r);

  // Convert from screen to window coordinates
  p.x -= r.left;
  p.y -= r.top;

  if (!IsTabStripVisible() &&
      ComputeResizeMode(p.x, p.y, r.Width(), r.Height()) == RM_UNDEFINED &&
      root_view_.GetViewForPoint(p) == frame_view_) {
    return HTCAPTION;
  }

  CPoint tsp(p);
  ChromeViews::View::ConvertPointToView(&root_view_, tabstrip_, &tsp);

  // If the mouse is over the tabstrip. Check if we should move the window or
  // capture the mouse.
  if (tabstrip_->CanProcessInputEvents() && tabstrip_->HitTest(tsp)) {
    // The top few pixels of a tab strip are a dropshadow - as we're pretty
    // starved of draggable area, let's give it to window dragging (this
    // also makes sense visually.
    if (!IsZoomed() && tsp.y < kTabShadowSize)
      return HTCAPTION;

    ChromeViews::View* v = tabstrip_->GetViewForPoint(tsp);
    // If there is not tab at this location, claim the hit was in the title
    // bar to get a move action.
    if (v == tabstrip_)
      return HTCAPTION;

    // If the view under mouse is a tab, check if the tab strip allows tab
    // dragging or not. If not, return caption to get window dragging.
    if (v->GetClassName() == Tab::kTabClassName &&
        !tabstrip_->HasAvailableDragActions())
      return HTCAPTION;
  } else {
    // The mouse is not above the tab strip. If there is no control under it,
    // let's move the window.
    if (ComputeResizeMode(p.x, p.y, r.Width(), r.Height()) == RM_UNDEFINED) {
      ChromeViews::View* v = root_view_.GetViewForPoint(p);
      if (v == frame_view_ || v == off_the_record_image_ || 
          v == distributor_logo_) {
        return HTCAPTION;
      }
    }
  }

  SetMsgHandled(FALSE);
  return 0;
}

void XPFrame::ArmOnMouseLeave() {
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

void XPFrame::OnInitMenu(HMENU menu) {
  BOOL is_iconic = IsIconic();
  BOOL is_zoomed = IsZoomed();

  if (is_iconic || is_zoomed) {
    ::EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | MF_ENABLED);
    ::EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    ::EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    if (is_iconic) {
      ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_ENABLED);
      ::EnableMenuItem(menu, SC_MINIMIZE, MF_BYCOMMAND | MF_DISABLED |
                       MF_GRAYED);
    } else {
      ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_DISABLED |
                       MF_GRAYED);
      ::EnableMenuItem(menu, SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
    }
  } else {
    ::EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    ::EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | MF_ENABLED);
    ::EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | MF_ENABLED);
    ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_ENABLED);
    ::EnableMenuItem(menu, SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
  }
}

void XPFrame::ShowSystemMenu(int x, int y) {
  HMENU system_menu = ::GetSystemMenu(*this, FALSE);
  int id = ::TrackPopupMenu(system_menu,
                            TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                            x,
                            y,
                            0,
                            *this,
                            NULL);
  if (id) {
    ::SendMessage(*this,
                  WM_SYSCOMMAND,
                  id,
                  0);
  }
}

LRESULT XPFrame::OnNCActivate(BOOL param) {
  if (ignore_ncactivate_) {
    ignore_ncactivate_ = false;
    return DefWindowProc(WM_NCACTIVATE, TRUE, 0);
  }
  SetMsgHandled(false);
  return 0;
}

BOOL XPFrame::OnPowerBroadcast(DWORD power_event, DWORD data) {
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

void XPFrame::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
  FrameUtil::NotifyTabsOfThemeChange(browser_);
}

LRESULT XPFrame::OnAppCommand(
    HWND w_param, short app_command, WORD device, int keystate) {
  if (browser_ && !browser_->ExecuteWindowsAppCommand(app_command))
    SetMsgHandled(FALSE);
  return 0;
}

void XPFrame::OnCommand(UINT notification_code,
                           int command_id,
                           HWND window) {
  if (browser_ && browser_->SupportsCommand(command_id)) {
    browser_->ExecuteCommand(command_id);
  } else {
    SetMsgHandled(FALSE);
  }
}

void XPFrame::OnSysCommand(UINT notification_code, CPoint click) {
  switch (notification_code) {
    case SC_CLOSE:
      Close();
      break;
    case SC_MAXIMIZE:
      ShowWindow(IsZoomed() ? SW_RESTORE : SW_MAXIMIZE);
      break;
    case IDC_TASKMANAGER:
      if (browser_)
        browser_->ExecuteCommand(IDC_TASKMANAGER);
      break;

    // Note that we avoid calling ShowWindow(SW_SHOWMINIMIZED) when we get a
    // minimized system command because doing so will minimize the window but
    // won't put the window at the bottom of the z-order window list. Instead,
    // we pass the minimized event to the default window procedure.
    case SC_MINIMIZE:
    default:
      // Use the default implementation for any other command.
      SetMsgHandled(FALSE);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Button:::ButtonListener
//
////////////////////////////////////////////////////////////////////////////////

void XPFrame::ButtonPressed(ChromeViews::BaseButton *sender) {
  switch (sender->GetTag()) {
    case MINIATURIZE_TAG:
      // We deliberately don't call ShowWindow(SW_SHOWMINIMIZED) directly
      // because doing that will minimize the window, but won't put the window
      // in the right z-order location.
      //
      // In order to minimize the window correctly, we need to post a system
      // command which will be passed to the default window procedure for
      // correct handling.
      ::PostMessage(*this, WM_SYSCOMMAND, SC_MINIMIZE, 0);
      break;
    case MAXIMIZE_TAG:
      ShowWindow(SW_MAXIMIZE);
      break;
    case RESTORE_TAG:
      ShowWindow(SW_RESTORE);
      break;
    case CLOSE_TAG:
      Close();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Window move and resize
//
////////////////////////////////////////////////////////////////////////////////

void XPFrame::BrowserDidPaint(HRGN rgn) {
  browser_paint_pending_ = false;
}

bool XPFrame::ShouldRefreshCurrentTabContents() {
  if (browser_->tabstrip_model()) {
    TabContents* tc = browser_->GetSelectedTabContents();
    if (tc) {
      HWND tc_hwnd = tc->GetContainerHWND();
      return !!(::GetWindowLongPtr(tc_hwnd, GWL_STYLE) & WS_CLIPCHILDREN);
    }
  }
  return false;
}

void XPFrame::StartWindowResize(ResizeMode mode, const CPoint &pt) {
  SetCapture();
  current_action_ = FA_RESIZING;
  current_resize_mode_ = mode;

  SetResizeCursor(mode);

  GetWindowRect(&previous_bounds_);
  drag_origin_ = pt;
  drag_origin_.x += previous_bounds_.left;
  drag_origin_.y += previous_bounds_.top;
}

void XPFrame::ProcessWindowResize(const CPoint &pt) {
  CPoint current(pt);
  CRect rect;
  CRect new_rect(previous_bounds_);
  CPoint initial(drag_origin_);
  GetWindowRect(&rect);

  current.x += rect.left;
  current.y += rect.top;

  switch (current_resize_mode_) {
    case RM_NORTH:
      new_rect.top = std::min(new_rect.bottom - minimum_size_.cy,
                              new_rect.top   + (current.y - initial.y));
      break;
    case RM_NORTH_EAST:
      new_rect.top = std::min(new_rect.bottom - minimum_size_.cy,
                              new_rect.top   + (current.y - initial.y));
      new_rect.right = std::max(new_rect.left + minimum_size_.cx,
                                new_rect.right + (current.x - initial.x));
      break;
    case RM_EAST:
      new_rect.right = std::max(new_rect.left + minimum_size_.cx,
                                new_rect.right + (current.x - initial.x));
      break;
    case RM_SOUTH_EAST:
      new_rect.right = std::max(new_rect.left + minimum_size_.cx,
                                new_rect.right + (current.x - initial.x));
      new_rect.bottom = std::max(new_rect.top + minimum_size_.cy,
                                 new_rect.bottom + (current.y - initial.y));
      break;
    case RM_SOUTH:
      new_rect.bottom = std::max(new_rect.top + minimum_size_.cy,
                                 new_rect.bottom + (current.y - initial.y));
      break;
    case RM_SOUTH_WEST:
      new_rect.left = std::min(new_rect.right - minimum_size_.cx,
                               new_rect.left + (current.x - initial.x));
      new_rect.bottom = std::max(new_rect.top + minimum_size_.cy,
                                 new_rect.bottom + (current.y - initial.y));
      break;
    case RM_WEST:
      new_rect.left = std::min(new_rect.right - minimum_size_.cx,
                               new_rect.left + (current.x - initial.x));
      break;
    case RM_NORTH_WEST:
      new_rect.left = std::min(new_rect.right - minimum_size_.cx,
                               new_rect.left + (current.x - initial.x));
      new_rect.top = std::min(new_rect.bottom - minimum_size_.cy,
                              new_rect.top + (current.y - initial.y));
      break;
  }

  if (!EqualRect(rect, new_rect)) {
    // Performing the refresh appears to be faster than simply calling
    // MoveWindow(... , TRUE)
    MoveWindow(new_rect.left,
               new_rect.top,
               new_rect.Width(),
               new_rect.Height(),
               FALSE);
    HWND h = GetDesktopWindow();
    HRGN rgn = ::CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
    HRGN rgn2 = ::CreateRectRgn(new_rect.left,
                                new_rect.top,
                                new_rect.right,
                                new_rect.bottom);

    // Erase the corners
    HRGN rgn3 = ::CreateRectRgn(rect.left,
                                rect.top,
                                rect.left + kCurvedCornerSize,
                                rect.top + kCurvedCornerSize);
    HRGN rgn4 = ::CreateRectRgn(rect.right - kCurvedCornerSize,
                                rect.top,
                                rect.right,
                                rect.top + kCurvedCornerSize);
    HRGN rgn5 = ::CreateRectRgn(new_rect.left,
                                new_rect.top,
                                new_rect.left + kCurvedCornerSize,
                                new_rect.top + kCurvedCornerSize);
    HRGN rgn6 = ::CreateRectRgn(new_rect.right - kCurvedCornerSize,
                                new_rect.top,
                                new_rect.right,
                                new_rect.top + kCurvedCornerSize);

    ::CombineRgn(rgn, rgn, rgn2, RGN_OR);
    ::CombineRgn(rgn, rgn, rgn3, RGN_OR);
    ::CombineRgn(rgn, rgn, rgn4, RGN_OR);
    ::CombineRgn(rgn, rgn, rgn5, RGN_OR);
    ::CombineRgn(rgn, rgn, rgn6, RGN_OR);

    ::RedrawWindow(h, NULL, rgn, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    ::DeleteObject(rgn);
    ::DeleteObject(rgn2);
    ::DeleteObject(rgn3);
    ::DeleteObject(rgn4);
    ::DeleteObject(rgn5);
    ::DeleteObject(rgn6);
  }
}

void XPFrame::StopWindowResize() {
  current_action_ = FA_NONE;
  ReleaseCapture();
}

XPFrame::ResizeMode XPFrame::ComputeResizeMode(int x,
                                               int y,
                                               int width,
                                               int height) {
  // Make sure we're not over a window control (they overlap our resize area).
  if (x >= min_button_->GetX() &&
      x < close_button_->GetX() + close_button_->GetWidth() &&
      y >= min_button_->GetY() &&
      y < min_button_->GetY() + min_button_->GetHeight()) {
    return RM_UNDEFINED;
  }

  ResizeMode mode = RM_UNDEFINED;

  // WEST / SOUTH_WEST / NORTH_WEST edge
  if (x < kResizeAreaSize) {
    if (y < kResizeAreaCornerSize) {
      mode = RM_NORTH_WEST;
    } else if (y >= (height - kResizeAreaCornerSize)) {
      mode = RM_SOUTH_WEST;
    } else {
      mode = RM_WEST;
    }
    // SOUTH_WEST / NORTH_WEST horizontal extensions
  } else if (x < kResizeAreaCornerSize) {
    if (y < kResizeAreaNorthSize) {
      mode = RM_NORTH_WEST;
    } else if (y >= (height - kResizeAreaSize)) {
      mode = RM_SOUTH_WEST;
    }
    // EAST / SOUTH_EAST / NORTH_EAST edge
  } else if (x >= (width - kResizeAreaSize)) {
    if (y < kResizeAreaCornerSize) {
      mode = RM_NORTH_EAST;
    } else if (y >= (height - kResizeAreaCornerSize)) {
      mode = RM_SOUTH_EAST;
    } else if (x >= (width - kResizeAreaSize)) {
      mode = RM_EAST;
    }
    // EAST / SOUTH_EAST / NORTH_EAST horizontal extensions
  } else if (x >= (width - kResizeAreaCornerSize)) {
    if (y < kResizeAreaNorthSize) {
      mode = RM_NORTH_EAST;
    } else if (y >= (height - kResizeAreaSize)) {
      mode = RM_SOUTH_EAST;
    }
    // NORTH edge
  } else if (y < kResizeAreaNorthSize) {
    mode = RM_NORTH;
    // SOUTH edge
  } else if (y >= (height - kResizeAreaSize)) {
    mode = RM_SOUTH;
  }

  return mode;
}

void XPFrame::SetResizeCursor(ResizeMode r_mode) {
  static ResizeCursor map[] = { RC_VERTICAL,  // unefined
                                RC_VERTICAL, RC_NESW, RC_HORIZONTAL, RC_NWSE,
                                RC_VERTICAL, RC_NESW, RC_HORIZONTAL, RC_NWSE };

  if (r_mode == RM_UNDEFINED) {
    if (previous_cursor_) {
      SetCursor(previous_cursor_);
      previous_cursor_ = NULL;
    }
  } else {
    HCURSOR cursor = g_resize_cursors[map[r_mode]];
    HCURSOR prev_cursor = SetCursor(cursor);
    if (prev_cursor != cursor) {
      previous_cursor_ = cursor;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// ViewContainer
//
////////////////////////////////////////////////////////////////////////////////

void XPFrame::GetBounds(CRect *out, bool including_frame) const {
  // We ignore including_frame because our XP frame doesn't have any NC area
  GetWindowRect(out);
}

bool XPFrame::IsMaximized() {
  return !!IsZoomed();
}

gfx::Rect XPFrame::GetBoundsForContentBounds(const gfx::Rect content_rect) {
  if (tab_contents_container_->GetX() == 0 &&
      tab_contents_container_->GetWidth() == 0) {
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
              (bounds.Width() - (p.x + tab_contents_container_->GetWidth())));
  r.set_height(p.y + content_rect.height() +
               (bounds.Height() - (p.y +
                                   tab_contents_container_->GetHeight())));
  return r;
}

void XPFrame::DetachFromBrowser() {
  browser_->tabstrip_model()->RemoveObserver(tabstrip_);
  browser_ = NULL;
}

void XPFrame::InfoBubbleShowing() {
  ignore_ncactivate_ = true;
  paint_as_active_ = true;
}

void XPFrame::InfoBubbleClosing() {
  paint_as_active_ = false;
  BrowserWindow::InfoBubbleClosing();
  // How we render the frame has changed, we need to force a paint otherwise
  // visually the user won't be able to tell.
  InvalidateRect(NULL, false);
}

ToolbarStarToggle* XPFrame::GetStarButton() const {
  return browser_view_->GetStarButton();
}

LocationBarView* XPFrame::GetLocationBarView() const {
  return browser_view_->GetLocationBarView();
}

GoButton* XPFrame::GetGoButton() const {
  return browser_view_->GetGoButton();
}

BookmarkBarView* XPFrame::GetBookmarkBarView() {
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

BrowserView* XPFrame::GetBrowserView() const {
  return browser_view_;
}

void XPFrame::Update(TabContents* contents, bool should_restore_state) {
  browser_view_->Update(contents, should_restore_state);
}

void XPFrame::ProfileChanged(Profile* profile) {
  browser_view_->ProfileChanged(profile);
}

void XPFrame::FocusToolbar() {
  browser_view_->FocusToolbar();
}

void XPFrame::MoveToFront(bool should_activate) {
  int flags = SWP_NOMOVE | SWP_NOSIZE;
  if (!should_activate) {
    flags |= SWP_NOACTIVATE;
  }
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, flags);
  SetForegroundWindow(*this);
}

HWND XPFrame::GetHWND() const {
  return m_hWnd;
}

void XPFrame::PaintNow(const CRect& update_rect) {
  if (!update_rect.IsRectNull() && IsVisible()) {
    RedrawWindow(update_rect,
                 NULL,
                 // While we don't seem to need RDW_NOERASE here for correctness
                 // (unlike Vista), I don't know whether it would hurt.
                 RDW_INVALIDATE | RDW_ALLCHILDREN);
  }
}

ChromeViews::RootView* XPFrame::GetRootView() {
  return const_cast<ChromeViews::RootView*>(&root_view_);
}

bool XPFrame::IsVisible() {
  return !!::IsWindowVisible(*this);
}

bool XPFrame::IsActive() {
  return win_util::IsWindowActive(*this);
}

bool XPFrame::ProcessMousePressed(const CPoint &pt,
                                  UINT flags,
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
    if (current_action_ != FA_FORWARDING) {
      current_action_ = FA_FORWARDING;
      SetCapture();
    }
    return TRUE;
  }
  return FALSE;
}

void XPFrame::ProcessMouseDragged(const CPoint &pt, UINT flags) {
  using namespace ChromeViews;
  MouseEvent drag_event(Event::ET_MOUSE_DRAGGED,
                        pt.x,
                        pt.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseDragged(drag_event);
}

void XPFrame::ProcessMouseReleased(const CPoint &pt, UINT flags) {
  using namespace ChromeViews;

  current_action_ = FA_NONE;
  ReleaseCapture();

  MouseEvent mouse_released(Event::ET_MOUSE_RELEASED,
                            pt.x,
                            pt.y,
                            Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseReleased(mouse_released, false);
}

void XPFrame::ProcessMouseMoved(const CPoint& pt, UINT flags) {
  using namespace ChromeViews;
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                        pt.x,
                        pt.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_.OnMouseMoved(mouse_move);
}

void XPFrame::ProcessMouseExited() {
  root_view_.ProcessOnMouseExited();
}

////////////////////////////////////////////////////////////////////////////////
//
// XPFrameView
//
////////////////////////////////////////////////////////////////////////////////

void XPFrame::XPFrameView::PaintFrameBorder(ChromeCanvas* canvas) {
  int width = GetWidth();
  int height = GetHeight();
  int x, y;
  x = 0;
  y = 0;

  static const SkBitmap * top_left_corner;
  static const SkBitmap * top_center;
  static const SkBitmap * top_right_corner;
  static const SkBitmap * left_side;
  static const SkBitmap * right_side;
  static const SkBitmap * bottom_left_corner;
  static const SkBitmap * bottom_center;
  static const SkBitmap * bottom_right_corner;

  SkBitmap** bitmaps = parent_->GetFrameBitmaps();

  if (parent_->PaintAsActive()) {
    top_left_corner = bitmaps[TOP_LEFT_CORNER];
    top_center = bitmaps[TOP_CENTER];
    top_right_corner = bitmaps[TOP_RIGHT_CORNER];
    left_side = bitmaps[LEFT_SIDE];
    right_side = bitmaps[RIGHT_SIDE];
    bottom_left_corner = bitmaps[BOTTOM_LEFT_CORNER];
    bottom_center = bitmaps[BOTTOM_CENTER];
    bottom_right_corner = bitmaps[BOTTOM_RIGHT_CORNER];
  } else {
    top_left_corner = bitmaps[DE_TOP_LEFT_CORNER];
    top_center = bitmaps[DE_TOP_CENTER];
    top_right_corner = bitmaps[DE_TOP_RIGHT_CORNER];
    left_side = bitmaps[DE_LEFT_SIDE];
    right_side = bitmaps[DE_RIGHT_SIDE];
    bottom_left_corner = bitmaps[DE_BOTTOM_LEFT_CORNER];
    bottom_center = bitmaps[DE_BOTTOM_CENTER];
    bottom_right_corner = bitmaps[DE_BOTTOM_RIGHT_CORNER];
  }

  int variable_width = width - top_left_corner->width() -
      top_right_corner->width();
  int variable_height = height - top_right_corner->height() -
      bottom_right_corner->height();

  // Top
  canvas->DrawBitmapInt(*top_left_corner, x, y);
  canvas->TileImageInt(*top_center,
                       x + top_left_corner->width(),
                       y,
                       variable_width,
                       top_center->height());
  x = width - top_right_corner->width();
  canvas->DrawBitmapInt(*top_right_corner, x, y);

  // Right side
  canvas->TileImageInt(*right_side,
                       x,
                       top_right_corner->height(),
                       right_side->width(),
                       variable_height);

  // Bottom
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width - bottom_right_corner->width(),
                        height - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_center,
                       bottom_left_corner->width(),
                       height - bottom_center->height(),
                       variable_width,
                       bottom_center->height());
  canvas->DrawBitmapInt(*bottom_left_corner,
                        0,
                        height - bottom_left_corner->height());

  // Left
  canvas->TileImageInt(*left_side,
                       0,
                       top_left_corner->height(),
                       left_side->width(),
                       variable_height);
}

void XPFrame::XPFrameView::PaintFrameBorderZoomed(ChromeCanvas* canvas) {
  int width = GetWidth();
  int height = GetHeight();

  static const SkBitmap * maximized_top;
  static const SkBitmap * maximized_bottom;

  SkBitmap** bitmaps = parent_->GetFrameBitmaps();
  if (parent_->PaintAsActive()) {
    maximized_top = bitmaps[TOP_CENTER];
    maximized_bottom = bitmaps[BOTTOM_CENTER];
  } else {
    maximized_top = bitmaps[DE_TOP_CENTER];
    maximized_bottom = bitmaps[DE_BOTTOM_CENTER];
  }

  canvas->TileImageInt(*maximized_top,
                       0,
                       0,
                       width,
                       maximized_top->height());
  canvas->TileImageInt(*maximized_bottom,
                       0,
                       height - maximized_bottom->height(),
                       width,
                       maximized_bottom->height());
}

void XPFrame::XPFrameView::PaintContentsBorder(ChromeCanvas* canvas,
                                               int x,
                                               int y,
                                               int w,
                                               int h) {
  SkBitmap** bitmaps = parent_->GetFrameBitmaps();

  int ro, bo;
  ro = x + w - bitmaps[CT_TOP_RIGHT_CORNER]->width();
  bo = y + h - bitmaps[CT_BOTTOM_RIGHT_CORNER]->height();
  int start_y;

  if (parent_->IsTabStripVisible() || parent_->IsToolBarVisible()) {
    int top_height = bitmaps[CT_TOP_RIGHT_CORNER]->height();
    canvas->DrawBitmapInt(*bitmaps[CT_TOP_LEFT_CORNER], x, y);

    canvas->TileImageInt(*bitmaps[CT_TOP_CENTER],
                         x + bitmaps[CT_TOP_LEFT_CORNER]->width(),
                         y,
                         w - bitmaps[CT_TOP_LEFT_CORNER]->width() -
                         bitmaps[CT_TOP_RIGHT_CORNER]->width(),
                         bitmaps[CT_TOP_CENTER]->height());

    canvas->DrawBitmapInt(*bitmaps[CT_TOP_RIGHT_CORNER], ro, y);
    start_y = y + bitmaps[CT_TOP_RIGHT_CORNER]->height();

    // If the toolbar is not visible, we need to draw a line at the top of the
    // actual contents.
    if (!parent_->IsToolBarVisible()) {
      canvas->FillRectInt(kSeparationLineColor,
                          x + bitmaps[CT_TOP_LEFT_CORNER]->width(),
                          y + kCollapsedToolbarHeight +
                          kToolbarOverlapVertOffset - kSeparationLineHeight,
                          w - bitmaps[CT_TOP_LEFT_CORNER]->width() -
                          bitmaps[CT_TOP_RIGHT_CORNER]->width(),
                          kSeparationLineHeight);
    }
  } else {
    int by = y - bitmaps[APP_TOP_LEFT]->height() + 1;
    canvas->DrawBitmapInt(*bitmaps[APP_TOP_LEFT], x, by);
    canvas->TileImageInt(*bitmaps[APP_TOP_CENTER],
                         x + bitmaps[APP_TOP_LEFT]->width(),
                         by,
                         w - bitmaps[APP_TOP_LEFT]->width() -
                         bitmaps[APP_TOP_RIGHT]->width(),
                         bitmaps[APP_TOP_CENTER]->height());
    canvas->DrawBitmapInt(*bitmaps[APP_TOP_RIGHT],
                          x + w - bitmaps[APP_TOP_RIGHT]->width(),
                          by);
    start_y = y + 1;
  }

  canvas->TileImageInt(*bitmaps[CT_RIGHT_SIDE],
                       ro,
                       start_y,
                       bitmaps[CT_RIGHT_SIDE]->width(),
                       bo - start_y);

  canvas->DrawBitmapInt(*bitmaps[CT_BOTTOM_RIGHT_CORNER],
                      x + w - bitmaps[CT_BOTTOM_RIGHT_CORNER]->width(),
                      bo);

  canvas->TileImageInt(*bitmaps[CT_BOTTOM_CENTER],
                     x + bitmaps[CT_BOTTOM_LEFT_CORNER]->width(),
                     bo,
                     w - (bitmaps[CT_BOTTOM_LEFT_CORNER]->width() +
                          bitmaps[CT_BOTTOM_RIGHT_CORNER]->width()),
                     bitmaps[CT_BOTTOM_CENTER]->height());

  canvas->DrawBitmapInt(*bitmaps[CT_BOTTOM_LEFT_CORNER], x, bo);

  canvas->TileImageInt(*bitmaps[CT_LEFT_SIDE],
                       x,
                       start_y,
                       bitmaps[CT_LEFT_SIDE]->width(),
                       bo - start_y);
}

void XPFrame::XPFrameView::PaintContentsBorderZoomed(ChromeCanvas* canvas,
                                                     int x,
                                                     int y,
                                                     int w) {
  SkBitmap** bitmaps = parent_->GetFrameBitmaps();
  canvas->TileImageInt(*bitmaps[CT_TOP_CENTER],
                       x,
                       y,
                       w,
                       bitmaps[CT_TOP_CENTER]->height());

  // If the toolbar is not visible, we need to draw a line at the top of the
  // actual contents.
  if (!parent_->IsToolBarVisible()) {
    canvas->FillRectInt(kSeparationLineColor, x, y + kCollapsedToolbarHeight +
                        kToolbarOverlapVertOffset - 1, w, 1);
  }
}

void XPFrame::XPFrameView::Paint(ChromeCanvas* canvas) {
  canvas->save();

  // When painting the border, exclude the contents area. This will prevent the
  // border bitmaps (which might be larger than the visible area) from coming
  // into the content area when there is no tab painted yet.
  int x = parent_->tab_contents_container_->GetX();
  int y = parent_->tab_contents_container_->GetY();
  SkRect clip;
  clip.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + parent_->tab_contents_container_->GetWidth()),
           SkIntToScalar(y + parent_->tab_contents_container_->GetHeight()));
  canvas->clipRect(clip, SkRegion::kDifference_Op);

  if (parent_->IsZoomed()) {
    PaintFrameBorderZoomed(canvas);
    int y;
    bool should_draw_separator = false;
    if (parent_->IsToolBarVisible()) {
      y = parent_->browser_view_->GetY();
    } else if (parent_->IsTabStripVisible()) {
      y = parent_->GetContentsYOrigin() - kCollapsedToolbarHeight -
          kToolbarOverlapVertOffset;
    } else {
      y = parent_->GetContentsYOrigin();
    }

    PaintContentsBorderZoomed(canvas, 0, y, GetWidth());
  } else {
    PaintFrameBorder(canvas);
    int y, height;
    if (parent_->IsToolBarVisible()) {
      y = parent_->browser_view_->GetY();
      height = GetHeight() - (parent_->browser_view_->GetY() +
                              kContentBorderVertBottomOffset);
    } else {
      if (parent_->IsTabStripVisible()) {
        y = parent_->GetContentsYOrigin() - kCollapsedToolbarHeight -
            kToolbarOverlapVertOffset;
      } else {
        y = parent_->GetContentsYOrigin();
      }
      height = GetHeight() - y - kContentBorderVertBottomOffset;
    }

    PaintContentsBorder(canvas, kContentBorderHorizOffset, y,
                        GetWidth() - (2 * kContentBorderHorizOffset),
                        height);
  }

  canvas->restore();
}

bool XPFrame::XPFrameView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_CLIENT;
  return true;
}

bool XPFrame::XPFrameView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    name->assign(accessible_name_);
    return true;
  }
  return false;
}

void XPFrame::XPFrameView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

bool XPFrame::XPFrameView::ShouldForwardToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  if (!FrameView::ShouldForwardToTabStrip(event))
    return false;
  if (parent_->IsZoomed() && event.GetX() >= parent_->min_button_->GetX() &&
      event.GetY() < (parent_->min_button_->GetY() +
                      parent_->min_button_->GetHeight())) {
    return false;
  }
  return true;
}

void XPFrame::ShelfVisibilityChanged() {
  ShelfVisibilityChangedImpl(browser_->GetSelectedTabContents());
}

void XPFrame::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    tab_contents_container_->set_fast_resize(true);
    ShelfVisibilityChanged();
    tab_contents_container_->set_fast_resize(false);
  } else {
    ShelfVisibilityChanged();
    tab_contents_container_->UpdateHWNDBounds();
  }
}

bool XPFrame::UpdateChildViewAndLayout(ChromeViews::View* new_view,
                                       ChromeViews::View** view) {
  DCHECK(view);
  if (*view == new_view) {
    // The views haven't changed, if the views pref changed schedule a layout.
    if (new_view) {
      CSize pref_size;
      new_view->GetPreferredSize(&pref_size);
      if (pref_size.cy != new_view->GetHeight())
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
    current_height = (*view)->GetHeight();
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
    CRect last_bounds;
    (*view)->GetBounds(&last_bounds);
    new_view->SetBounds(last_bounds.left, last_bounds.top,
                        last_bounds.Width(), last_bounds.Height());
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

void XPFrame::SetWindowTitle(const std::wstring& title) {
  SetWindowText(title.c_str());
}

void XPFrame::Activate() {
  if (IsIconic())
    ShowWindow(SW_RESTORE);
  MoveToFront(true);
}

void XPFrame::FlashFrame() {
  FLASHWINFO flash_window_info;
  flash_window_info.cbSize = sizeof(FLASHWINFO);
  flash_window_info.hwnd = GetHWND();
  flash_window_info.dwFlags = FLASHW_ALL;
  flash_window_info.uCount = 4;
  flash_window_info.dwTimeout = 0;
  ::FlashWindowEx(&flash_window_info);
}

void XPFrame::ShowTabContents(TabContents* selected_contents) {
  tab_contents_container_->SetTabContents(selected_contents);

  // Force a LoadingStateChanged notification because the TabContents
  // could be loading (such as when the user unconstrains a tab.
  if (selected_contents && selected_contents->delegate())
    selected_contents->delegate()->LoadingStateChanged(selected_contents);

  ShelfVisibilityChangedImpl(selected_contents);
}

TabStrip* XPFrame::GetTabStrip() const {
  return tabstrip_;
}

gfx::Rect XPFrame::GetNormalBounds() {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool ret = !!GetWindowPlacement(&wp);
  DCHECK(ret);
  return gfx::Rect(wp.rcNormalPosition);
}

void XPFrame::ContinueDetachConstrainedWindowDrag(const gfx::Point& mouse_pt,
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
  if (frame_component == HTCAPTION) {
    // XPFrame uses windows' standard move code, so this works.
    PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, reinterpret_cast<LPARAM>(&pts));
  } else {
    // Because xpframe does its own resizing, and does not respond properly to
    // WM_NCHITTEST, there's no reliable way for us to handle other frame
    // component types. Alas. This will be corrected when XPFrame subclasses
    // ChromeViews::CustomFrameWindow, some day.
  }
}

void XPFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  // First we need to ensure everything has an initial size. Currently, the
  // window has the wrong size, but that's OK, doing this will allow us to
  // figure out how big all the UI bits are.
  Layout();

  // Then we calculate the size of the window chrome, this is the stuff that
  // needs to be positioned around the edges of contents_bounds.
  CRect bounds;
  tab_contents_container_->GetBounds(&bounds);
  CRect cr;
  GetClientRect(&cr);
  int toolbar_height = bounds.top;
  int left_edge_width = bounds.left;
  int right_edge_width = cr.Width() - bounds.right;
  int bottom_edge_height = cr.Height() - bounds.bottom;

  // Now resize the window. This will result in Layout() getting called again
  // and the contents getting sized to the value specified in |contents_bounds|
  SetWindowPos(NULL, contents_bounds.x() - left_edge_width,
               contents_bounds.y() - toolbar_height,
               contents_bounds.width() + left_edge_width + right_edge_width,
               contents_bounds.height() + toolbar_height + bottom_edge_height,
               SWP_NOZORDER | SWP_NOACTIVATE);
}

bool XPFrame::ShouldWorkAroundAutoHideTaskbar() {
  // Check the command line flag to see if we want to prevent
  // the work around for maximize and auto-hide task bar.
  // See bug#861590
  static bool should_work_around_auto_hide_taskbar = true;
  return should_work_around_auto_hide_taskbar;
}

void XPFrame::SetIsOffTheRecord(bool value) {
  is_off_the_record_ = value;
}

void XPFrame::DestroyBrowser() {
  // TODO(beng): (Cleanup) tidy this up, just fixing the build red for now.
  // Need to do this first, before the browser_ is deleted and we can't remove
  // the tabstrip from the model's observer list because the model was
  // destroyed with browser_.
  if (browser_) {
    browser_->tabstrip_model()->RemoveObserver(tabstrip_);
    delete browser_;
    browser_ = NULL;
  }
}

void XPFrame::ShelfVisibilityChangedImpl(TabContents* current_tab) {
  // Coalesce layouts.
  bool changed = false;

  ChromeViews::View* new_shelf = NULL;
  if (current_tab && current_tab->IsDownloadShelfVisible())
    new_shelf = current_tab->GetDownloadShelfView();
  changed |= UpdateChildViewAndLayout(new_shelf, &shelf_view_);

  ChromeViews::View* new_info_bar = NULL;
  if (current_tab && current_tab->IsInfoBarVisible())
    new_info_bar = current_tab->GetInfoBarView();
  changed |= UpdateChildViewAndLayout(new_info_bar, &info_bar_view_);

  ChromeViews::View* new_bookmark_bar_view = NULL;
  if (SupportsBookmarkBar() && current_tab)
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
