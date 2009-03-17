// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_view.h"

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/encoding_menu_controller_delegate.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/about_chrome_view.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/bookmark_bubble_view.h"
#include "chrome/browser/views/bookmark_manager_view.h"
#include "chrome/browser/views/bug_report_view.h"
#include "chrome/browser/views/clear_browsing_data.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/fullscreen_exit_bubble.h"
#include "chrome/browser/views/html_dialog_view.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/infobars/infobar_container.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/new_profile_dialog.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/select_profile_dialog.h"
#include "chrome/browser/views/status_bubble_views.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/native_scroll_bar.h"
#include "chrome/views/view.h"
#include "chrome/views/widget/hwnd_notification_source.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/window/non_client_view.h"
#include "chrome/views/window/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"


using base::TimeDelta;

// static
SkBitmap BrowserView::default_favicon_;
SkBitmap BrowserView::otr_avatar_;
// The vertical overlap between the TabStrip and the Toolbar.
static const int kToolbarTabStripVerticalOverlap = 3;
// The visible height of the shadow above the tabs. Clicks in this area are
// treated as clicks to the frame, rather than clicks to the tab.
static const int kTabShadowSize = 2;
// The height of the status bubble.
static const int kStatusBubbleHeight = 20;
// The overlap of the status bubble with the window frame.
static const int kStatusBubbleOverlap = 1;
// An offset distance between certain toolbars and the toolbar that preceded
// them in layout.
static const int kSeparationLineHeight = 1;
// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
static const wchar_t* kBrowserViewKey = L"__BROWSER_VIEW__";
// How frequently we check for hung plugin windows.
static const int kDefaultHungPluginDetectFrequency = 2000;
// How long do we wait before we consider a window hung (in ms).
static const int kDefaultPluginMessageResponseTimeout = 30000;
// The number of milliseconds between loading animation frames.
static const int kLoadingAnimationFrameTimeMs = 30;
// The amount of space we expect the window border to take up.
static const int kWindowBorderWidth = 5;

// If not -1, windows are shown with this state.
static int explicit_show_state = -1;

// Returned from BrowserView::GetClassName.
static const char kBrowserViewClassName[] = "browser/views/BrowserView";

static const struct {
  bool separator;
  int command;
  int label;
} kMenuLayout[] = {
  { true, 0, 0 },
  { false, IDC_TASK_MANAGER, IDS_TASK_MANAGER },
  { true, 0, 0 },
  { false, IDC_ENCODING_MENU, IDS_ENCODING_MENU },
  { false, IDC_ZOOM_MENU, IDS_ZOOM_MENU },
  { false, IDC_PRINT, IDS_PRINT },
  { false, IDC_SAVE_PAGE, IDS_SAVE_PAGE },
  { false, IDC_FIND, IDS_FIND },
  { true, 0, 0 },
  { false, IDC_PASTE, IDS_PASTE },
  { false, IDC_COPY, IDS_COPY },
  { false, IDC_CUT, IDS_CUT },
  { true, 0, 0 },
  { false, IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE },
  { false, IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB },
  { false, IDC_COPY_URL, IDS_APP_MENU_COPY_URL },
  { false, IDC_DUPLICATE_TAB, IDS_APP_MENU_DUPLICATE_APP_WINDOW },
  { true, 0, 0 },
  { false, IDC_RELOAD, IDS_APP_MENU_RELOAD },
  { false, IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD },
  { false, IDC_BACK, IDS_CONTENT_CONTEXT_BACK }
};

///////////////////////////////////////////////////////////////////////////////
// ResizeCorner, private:

class ResizeCorner : public views::View {
 public:
  ResizeCorner() { }

  virtual void Paint(ChromeCanvas* canvas) {
    BrowserView* browser = GetBrowserView();
    if (browser && !browser->CanCurrentlyResize())
      return;

    SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_TEXTAREA_RESIZER);
    bitmap->buildMipMap(false);
    bool rtl_dir = (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
    if (rtl_dir) {
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
      canvas->save();
    }
    canvas->DrawBitmapInt(*bitmap, width() - bitmap->width(),
                          height() - bitmap->height());
    if (rtl_dir)
      canvas->restore();
  }

  static gfx::Size GetSize() {
    // This is disabled until we find what makes us slower when we let
    // WebKit know that we have a resizer rect...
    // return gfx::Size(views::NativeScrollBar::GetVerticalScrollBarWidth(),
    //     views::NativeScrollBar::GetHorizontalScrollBarHeight());
    return gfx::Size();
  }

  virtual gfx::Size GetPreferredSize() {
    BrowserView* browser = GetBrowserView();
    return (browser && !browser->CanCurrentlyResize()) ?
        gfx::Size() : GetSize();
  }

  virtual void Layout() {
    views::View* parent_view = GetParent();
    if (parent_view) {
      gfx::Size ps = GetPreferredSize();
      // No need to handle Right to left text direction here,
      // our parent must take care of it for us...
      SetBounds(parent_view->width() - ps.width(),
                parent_view->height() - ps.height(), ps.width(), ps.height());
    }
  }

 private:
  // Returns the BrowserView we're displayed in. Returns NULL if we're not
  // currently in a browser view.
  BrowserView* GetBrowserView() {
    View* browser = GetAncestorWithClassName(kBrowserViewClassName);
    return browser ? static_cast<BrowserView*>(browser) : NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(ResizeCorner);
};


///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

// static
void BrowserView::SetShowState(int state) {
  explicit_show_state = state;
}

BrowserView::BrowserView(Browser* browser)
    : views::ClientView(NULL, NULL),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      active_download_shelf_(NULL),
      toolbar_(NULL),
      infobar_container_(NULL),
      find_bar_y_(0),
      contents_container_(NULL),
      initialized_(false),
      fullscreen_(false),
      ignore_layout_(false),
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0)
#ifdef CHROME_PERSONALIZATION
      , personalization_enabled_(false),
      personalization_(NULL)
#endif
      {
  InitClass();
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserView::~BrowserView() {
  browser_->tabstrip_model()->RemoveObserver(this);

  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);
}

// static
BrowserView* BrowserView::GetBrowserViewForHWND(HWND window) {
  if (IsWindow(window)) {
    HANDLE data = GetProp(window, kBrowserViewKey);
    if (data)
      return reinterpret_cast<BrowserView*>(data);
  }
  return NULL;
}

int BrowserView::GetShowState() const {
  if (explicit_show_state != -1)
    return explicit_show_state;

  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

void BrowserView::WindowMoved() {
  // Cancel any tabstrip animations, some of them may be invalidated by the
  // window being repositioned.
  // Comment out for one cycle to see if this fixes dist tests.
  // tabstrip_->DestroyDragController();

  status_bubble_->Reposition();

  BookmarkBubbleView::Hide();

  // Close the omnibox popup, if any.
  if (toolbar_->GetLocationBarView())
    toolbar_->GetLocationBarView()->location_entry()->ClosePopup();
}

void BrowserView::WindowMoveOrResizeStarted() {
  TabContents* tab_contents = GetSelectedTabContents();
  if (tab_contents && tab_contents->AsWebContents())
    tab_contents->AsWebContents()->WindowMoveOrResizeStarted();
}

bool BrowserView::CanCurrentlyResize() const {
  return !IsMaximized() && !IsFullscreen();
}

gfx::Rect BrowserView::GetToolbarBounds() const {
  return toolbar_->bounds();
}

gfx::Rect BrowserView::GetClientAreaBounds() const {
  gfx::Rect container_bounds = contents_container_->bounds();
  gfx::Point container_origin = container_bounds.origin();
  ConvertPointToView(this, GetParent(), &container_origin);
  container_bounds.set_origin(container_origin);
  return container_bounds;
}

bool BrowserView::ShouldFindBarBlendWithBookmarksBar() const {
  if (bookmark_bar_view_.get())
    return bookmark_bar_view_->IsAlwaysShown();
  return false;
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  // This function returns the area the Find Bar can be laid out within. This
  // basically implies the "user-perceived content area" of the browser window
  // excluding the vertical scrollbar. This is not quite so straightforward as
  // positioning based on the TabContentsContainerView since the BookmarkBarView
  // may be visible but not persistent (in the New Tab case) and we position
  // the Find Bar over the top of it in that case since the BookmarkBarView is
  // not _visually_ connected to the Toolbar.

  // First determine the bounding box of the content area in Widget coordinates.
  gfx::Rect bounding_box(contents_container_->bounds());

  gfx::Point topleft;
  views::View::ConvertPointToWidget(contents_container_, &topleft);
  bounding_box.set_origin(topleft);

  // Adjust the position and size of the bounding box by the find bar offset
  // calculated during the last Layout.
  int height_delta = find_bar_y_ - bounding_box.y();
  bounding_box.set_y(find_bar_y_);
  bounding_box.set_height(std::max(0, bounding_box.height() + height_delta));

  // Finally decrease the width of the bounding box by the width of the vertical
  // scroll bar.
  int scrollbar_width = views::NativeScrollBar::GetVerticalScrollBarWidth();
  bounding_box.set_width(std::max(0, bounding_box.width() - scrollbar_width));
  if (UILayoutIsRightToLeft())
    bounding_box.set_x(bounding_box.x() + scrollbar_width);

  return bounding_box;
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return IsTabStripVisible() ? tabstrip_->GetPreferredSize().height() : 0;
}

bool BrowserView::IsToolbarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

bool BrowserView::IsTabStripVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::ShouldShowOffTheRecordAvatar() const {
  return IsOffTheRecord() && IsBrowserTypeNormal();
}

bool BrowserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  DCHECK(accelerator_table_.get());
  std::map<views::Accelerator, int>::const_iterator iter =
      accelerator_table_->find(accelerator);
  DCHECK(iter != accelerator_table_->end());

  int command_id = iter->second;
  if (browser_->command_updater()->SupportsCommand(command_id) &&
      browser_->command_updater()->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

bool BrowserView::GetAccelerator(int cmd_id, views::Accelerator* accelerator) {
  std::map<views::Accelerator, int>::iterator it =
      accelerator_table_->begin();
  for (; it != accelerator_table_->end(); ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}

bool BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  if (AppModalDialogQueue::HasActiveDialog()) {
    Browser* active_browser = BrowserList::GetLastActive();
    if (active_browser && (browser_ != active_browser)) {
      active_browser->window()->FlashFrame();
      active_browser->window()->Activate();
    }
    AppModalDialogQueue::ActivateModalDialog();
    return true;
  }
  return false;
}

void BrowserView::ActivationChanged(bool activated) {
  if (activated)
    BrowserList::SetLastActive(browser_.get());
}

TabContents* BrowserView::GetSelectedTabContents() const {
  return browser_->GetSelectedTabContents();
}

SkBitmap BrowserView::GetOTRAvatarIcon() {
  if (otr_avatar_.isNull()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    otr_avatar_ = *rb.GetBitmapNamed(IDR_OTR_ICON);
  }
  return otr_avatar_;
}

void BrowserView::PrepareToRunSystemMenu(HMENU menu) {
  for (int i = 0; i < arraysize(kMenuLayout); ++i) {
    int command = kMenuLayout[i].command;
    // |command| can be zero on submenu items (IDS_ENCODING,
    // IDS_ZOOM) and on separators.
    if (command != 0) {
      system_menu_->EnableMenuItemByID(
          command,
          browser_->command_updater()->IsCommandEnabled(command));
    }
  }
}

// static
void BrowserView::RegisterBrowserViewPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kPluginMessageResponseTimeout,
                             kDefaultPluginMessageResponseTimeout);
  prefs->RegisterIntegerPref(prefs::kHungPluginDetectFrequency,
                             kDefaultHungPluginDetectFrequency);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Init() {
  // Stow a pointer to this object onto the window handle so that we can get
  // at it later when all we have is a HWND.
  SetProp(GetWidget()->GetNativeView(), kBrowserViewKey, this);

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    InitHangMonitor();
  }

  LoadAccelerators();
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  tabstrip_ = new TabStrip(browser_->tabstrip_model());
  tabstrip_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  AddChildView(tabstrip_);

  toolbar_ = new BrowserToolbarView(browser_.get());
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  infobar_container_ = new InfoBarContainer(this);
  AddChildView(infobar_container_);

  FindBarWin* find_bar_win = new FindBarWin(this);

  find_bar_controller_.reset(new FindBarController(find_bar_win));
  find_bar_win->set_find_bar_controller(find_bar_controller_.get());

  contents_container_ = new TabContentsContainerView;
  set_contents_view(contents_container_);
  AddChildView(contents_container_);

  status_bubble_.reset(new StatusBubbleViews(GetWidget()));

#ifdef CHROME_PERSONALIZATION
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  EnablePersonalization(!command_line.HasSwitch(switches::kDisableP13n));
  if (IsPersonalizationEnabled()) {
    personalization_ = Personalization::CreateFramePersonalization(
        browser_->profile(), this);
  }
#endif

  InitSystemMenu();
}

void BrowserView::Show() {
  // If the window is already visible, just activate it.
  if (frame_->IsVisible()) {
    frame_->Activate();
    return;
  }

  // Setting the focus doesn't work when the window is invisible, so any focus
  // initialization that happened before this will be lost.
  //
  // We really "should" restore the focus whenever the window becomes unhidden,
  // but I think initializing is the only time where this can happen where
  // there is some focus change we need to pick up, and this is easier than
  // plumbing through an un-hide message all the way from the frame.
  //
  // If we do find there are cases where we need to restore the focus on show,
  // that should be added and this should be removed.
  TabContents* selected_tab_contents = GetSelectedTabContents();
  if (selected_tab_contents && selected_tab_contents->AsWebContents())
    selected_tab_contents->AsWebContents()->view()->RestoreFocus();

  frame_->Show();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  frame_->SetBounds(bounds);
}

void BrowserView::Close() {
  frame_->Close();
}

void BrowserView::Activate() {
  frame_->Activate();
}

bool BrowserView::IsActive() const {
  return frame_->IsActive();
}

void BrowserView::FlashFrame() {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = frame_->GetNativeView();
  fwi.dwFlags = FLASHW_ALL;
  fwi.uCount = 4;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);
}

gfx::NativeWindow BrowserView::GetNativeHandle() {
  return GetWidget()->GetNativeView();
}

BrowserWindowTesting* BrowserView::GetBrowserWindowTesting() {
  return this;
}

StatusBubble* BrowserView::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserView::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    contents_container_->set_fast_resize(true);
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_container_->set_fast_resize(false);
  } else {
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_container_->UpdateHWNDBounds();
  }
}

void BrowserView::UpdateTitleBar() {
  frame_->UpdateWindowTitle();
  if (ShouldShowWindowIcon())
    frame_->UpdateWindowIcon();
}

void BrowserView::UpdateLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_timer_.Start(
          TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs), this,
          &BrowserView::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserView::SetStarredState(bool is_starred) {
  toolbar_->star_button()->SetToggled(is_starred);
}

gfx::Rect BrowserView::GetNormalBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (fullscreen_)
    return gfx::Rect(saved_window_info_.window_rect);

  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool ret = !!GetWindowPlacement(frame_->GetNativeView(), &wp);
  DCHECK(ret);
  return gfx::Rect(wp.rcNormalPosition);
}

bool BrowserView::IsMaximized() const {
  return frame_->IsMaximized();
}

void BrowserView::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;  // Nothing to do.

  // Move focus out of the location bar if necessary.
  LocationBarView* location_bar = toolbar_->GetLocationBarView();
  if (!fullscreen_) {
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    if (focus_manager->GetFocusedView() == location_bar)
      focus_manager->ClearFocus();
  }
  AutocompleteEditViewWin* edit_view =
      static_cast<AutocompleteEditViewWin*>(location_bar->location_entry());

  // Toggle fullscreen mode.
  fullscreen_ = fullscreen;

  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly
  ignore_layout_ = true;
  frame_->set_force_hidden(true);
  if (fullscreen_) {
    // If we don't hide the edit and force it to not show until we come out of
    // fullscreen, then if the user was on the New Tab Page, the edit contents
    // will appear atop the web contents once we go into fullscreen mode.  This
    // has something to do with how we move the main window while it's hidden;
    // if we don't hide the main window below, we don't get this problem.
    edit_view->set_force_hidden(true);
    ShowWindow(edit_view->m_hWnd, SW_HIDE);
  }
  frame_->Hide();

  // Notify bookmark bar, so it can set itself to the appropriate drawing state.
  if (bookmark_bar_view_.get())
    bookmark_bar_view_->OnFullscreenToggled(fullscreen_);

  // Size/position/style window appropriately.
  views::Widget* widget = GetWidget();
  HWND hwnd = widget->GetNativeView();
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY),
                 &monitor_info);
  gfx::Rect monitor_rect(monitor_info.rcMonitor);
  if (fullscreen_) {
    // Save current window information.  We force the window into restored mode
    // before going fullscreen because Windows doesn't seem to hide the
    // taskbar if the window is in the maximized state.
    saved_window_info_.maximized = IsMaximized();
    if (saved_window_info_.maximized)
      frame_->ExecuteSystemMenuCommand(SC_RESTORE);
    saved_window_info_.style = GetWindowLong(hwnd, GWL_STYLE);
    saved_window_info_.ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    GetWindowRect(hwnd, &saved_window_info_.window_rect);

    // Set new window style and size.
    SetWindowLong(hwnd, GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd, GWL_EXSTYLE,
                  saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME |
                  WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    SetWindowPos(hwnd, NULL, monitor_rect.x(), monitor_rect.y(),
                 monitor_rect.width(), monitor_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    fullscreen_bubble_.reset(new FullscreenExitBubble(widget, browser_.get()));
  } else {
    // Hide the fullscreen bubble as soon as possible, since the calls below can
    // take enough time for the user to notice.
    fullscreen_bubble_.reset();

    // Reset original window style and size.  The multiple window size/moves
    // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
    // repainted.  Better-looking methods welcome.
    gfx::Rect new_rect(saved_window_info_.window_rect);
    SetWindowLong(hwnd, GWL_STYLE, saved_window_info_.style);
    SetWindowLong(hwnd, GWL_EXSTYLE, saved_window_info_.ex_style);
    SetWindowPos(hwnd, NULL, new_rect.x(), new_rect.y(), new_rect.width(),
                 new_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (saved_window_info_.maximized)
      frame_->ExecuteSystemMenuCommand(SC_MAXIMIZE);

    // Show the edit again since we're no longer in fullscreen mode.
    edit_view->set_force_hidden(false);
    ShowWindow(edit_view->m_hWnd, SW_SHOW);
  }

  // Undo our anti-jankiness hacks and force the window to relayout now that
  // it's in its final position.
  ignore_layout_ = false;
  Layout();
  frame_->set_force_hidden(false);
  ShowWindow(hwnd, SW_SHOW);
}

bool BrowserView::IsFullscreen() const {
  return fullscreen_;
}

LocationBar* BrowserView::GetLocationBar() const {
  return toolbar_->GetLocationBarView();
}

void BrowserView::SetFocusToLocationBar() {
  LocationBarView* location_bar = toolbar_->GetLocationBarView();
  if (location_bar->IsFocusable()) {
    location_bar->FocusLocation();
  } else {
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
  }
}

void BrowserView::UpdateStopGoState(bool is_loading) {
  toolbar_->GetGoButton()->ChangeMode(
      is_loading ? GoButton::MODE_STOP : GoButton::MODE_GO);
}

void BrowserView::UpdateToolbar(TabContents* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView::FocusToolbar() {
  // Do not restore the button that previously had accessibility focus, if
  // focus is set by using the toolbar focus keyboard shortcut.
  toolbar_->set_acc_focused_view(NULL);
  // HACK: Do not use RequestFocus() here, as the toolbar is not marked as
  // "focusable".  Instead bypass the sanity check in RequestFocus() and just
  // force it to focus, which will do the right thing.
  GetRootView()->FocusView(toolbar_);
}

void BrowserView::DestroyBrowser() {
  // Explicitly delete the BookmarkBarView now. That way we don't have to
  // worry about the BookmarkBarView potentially outliving the Browser &
  // Profile.
  bookmark_bar_view_.reset();
  browser_.reset();
}

bool BrowserView::IsBookmarkBarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      active_bookmark_bar_ &&
      (active_bookmark_bar_->GetPreferredSize().height() != 0);
}

gfx::Rect BrowserView::GetRootWindowResizerRect() const {
  if (!CanCurrentlyResize())
    return gfx::Rect();

  // We don't specify a resize corner size if we have a bottom shelf either.
  // This is because we take care of drawing the resize corner on top of that
  // shelf, so we don't want others to do it for us in this case.
  // Currently, the only visible bottom shelf is the download shelf.
  // Other tests should be added here if we add more bottom shelves.
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (current_tab && current_tab->IsDownloadShelfVisible()) {
    DownloadShelf* download_shelf = current_tab->GetDownloadShelf();
    if (download_shelf && download_shelf->IsShowing())
      return gfx::Rect();
  }

  gfx::Rect client_rect = contents_container_->bounds();
  gfx::Size resize_corner_size = ResizeCorner::GetSize();
  int x = client_rect.width() - resize_corner_size.width();
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    x = 0;
  return gfx::Rect(x, client_rect.height() - resize_corner_size.height(),
                   resize_corner_size.width(), resize_corner_size.height());
}

void BrowserView::DisableInactiveFrame() {
  frame_->DisableInactiveRendering();
}

void BrowserView::ToggleBookmarkBar() {
  BookmarkBarView::ToggleWhenVisible(browser_->profile());
}

void BrowserView::ShowFindBar() {
  find_bar_controller_->Show();
}

void BrowserView::ShowAboutChromeDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetNativeView(), gfx::Rect(),
      new AboutChromeView(browser_->profile()))->Show();
}

void BrowserView::ShowBookmarkManager() {
  BookmarkManagerView::Show(browser_->profile());
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  toolbar_->star_button()->ShowStarBubble(url, !already_bookmarked);
}

void BrowserView::ShowReportBugDialog() {
  // Retrieve the URL for the current tab (if any) and tell the BugReportView
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!current_tab)
    return;

  BugReportView* bug_report_view = new BugReportView(browser_->profile(),
                                                     current_tab);

  if (current_tab->controller()->GetLastCommittedEntry()) {
    if (current_tab->type() == TAB_CONTENTS_WEB) {
      // URL for the current page
      bug_report_view->SetUrl(
          current_tab->controller()->GetActiveEntry()->url());
    }
  }

  // retrieve the application version info
  std::wstring version;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    version = version_info->product_name() + L" - " +
    version_info->file_version() +
    L" (" + version_info->last_change() + L")";
  }
  bug_report_view->set_version(version);

  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins)
  std::vector<unsigned char> *screenshot_png = new std::vector<unsigned char>;
  win_util::GrabWindowSnapshot(GetWidget()->GetNativeView(), screenshot_png);
  // the BugReportView takes ownership of the png data, and will dispose of
  // it in its destructor.
  bug_report_view->set_png_data(screenshot_png);

  // Create and show the dialog
  views::Window::CreateChromeWindow(GetWidget()->GetNativeView(), gfx::Rect(),
                                    bug_report_view)->Show();
}

void BrowserView::ShowClearBrowsingDataDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetNativeView(), gfx::Rect(),
      new ClearBrowsingDataView(browser_->profile()))->Show();
}

void BrowserView::ShowImportDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetNativeView(), gfx::Rect(),
      new ImporterView(browser_->profile()))->Show();
}

void BrowserView::ShowSearchEnginesDialog() {
  KeywordEditorView::Show(browser_->profile());
}

void BrowserView::ShowPasswordManager() {
  PasswordManagerView::Show(browser_->profile());
}

void BrowserView::ShowSelectProfileDialog() {
  SelectProfileDialog::RunDialog();
}

void BrowserView::ShowNewProfileDialog() {
  NewProfileDialog::RunDialog();
}

void BrowserView::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                 void* parent_window) {
  HWND parent_hwnd = reinterpret_cast<HWND>(parent_window);
  parent_hwnd = parent_hwnd ? parent_hwnd : GetWidget()->GetNativeView();
  HtmlDialogView* html_view = new HtmlDialogView(browser_.get(),
                                                 browser_->profile(),
                                                 delegate);
  views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), html_view);
  html_view->InitDialog();
  html_view->window()->Show();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindowTesting implementation:

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_->GetLocationBarView();
}

bool BrowserView::GetFindBarWindowInfo(gfx::Point* position,
                                       bool* fully_visible) const {
  FindBarWin* find_bar_win = NULL;
  if (find_bar_controller_.get()) {
    find_bar_win = static_cast<FindBarWin*>(
        find_bar_controller_->get_find_bar());
    DCHECK(find_bar_win);
  }

  CRect window_rect;
  if (!find_bar_controller_.get() ||
      !::IsWindow(find_bar_win->GetNativeView()) ||
      !::GetWindowRect(find_bar_win->GetNativeView(), &window_rect)) {
    *position = gfx::Point(0, 0);
    *fully_visible = false;
    return false;
  }

  *position = gfx::Point(window_rect.TopLeft().x, window_rect.TopLeft().y);
  *fully_visible = find_bar_win->IsVisible() && !find_bar_win->IsAnimating();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, NotificationObserver implementation:

void BrowserView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED &&
      *Details<std::wstring>(details).ptr() == prefs::kShowBookmarkBar) {
    if (MaybeShowBookmarkBar(browser_->GetSelectedTabContents()))
      Layout();
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::TabDetachedAt(TabContents* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetSelectedTabContents() will return NULL or something else.
  if (index == browser_->tabstrip_model()->selected_index()) {
    // We need to reset the current tab contents to NULL before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected TabContents when it is removed.
    infobar_container_->ChangeTabContents(NULL);
    contents_container_->SetTabContents(NULL);
    // When dragging the last TabContents out of a window there is no selection
    // notification that causes the find bar for that window to be un-registered
    // for notifications from this TabContents.
    find_bar_controller_->ChangeWebContents(NULL);
  }
}

void BrowserView::TabSelectedAt(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) {
  DCHECK(old_contents != new_contents);

  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (old_contents && !old_contents->is_being_destroyed() &&
      old_contents->AsWebContents())
    old_contents->AsWebContents()->view()->StoreFocus();

  // Update various elements that are interested in knowing the current
  // TabContents.
  infobar_container_->ChangeTabContents(new_contents);
  contents_container_->SetTabContents(new_contents);
  // TODO(beng): This should be called automatically by SetTabContents, but I
  //             am striving for parity now rather than cleanliness. This is
  //             required to make features like Duplicate Tab, Undo Close Tab,
  //             etc not result in sad tab.
  new_contents->DidBecomeSelected();
  if (BrowserList::GetLastActive() == browser_ &&
      !browser_->tabstrip_model()->closing_all() &&
      new_contents->AsWebContents()) {
    new_contents->AsWebContents()->view()->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);

  if (find_bar_controller_.get())
    find_bar_controller_->ChangeWebContents(new_contents->AsWebContents());
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::WindowDelegate implementation:

bool BrowserView::CanResize() const {
  return true;
}

bool BrowserView::CanMaximize() const {
  return true;
}

bool BrowserView::IsModal() const {
  return false;
}

std::wstring BrowserView::GetWindowTitle() const {
  return browser_->GetCurrentPageTitle();
}

views::View* BrowserView::GetInitiallyFocusedView() {
  // We set the frame not focus on creation so this should never be called.
  NOTREACHED();
  return NULL;
}

bool BrowserView::ShouldShowWindowTitle() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

SkBitmap BrowserView::GetWindowIcon() {
  if (browser_->type() & Browser::TYPE_APP)
    return browser_->GetCurrentPageIcon();
  return SkBitmap();
}

bool BrowserView::ShouldShowWindowIcon() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.

  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  if (browser_->command_updater()->SupportsCommand(command_id)) {
    if (browser_->command_updater()->IsCommandEnabled(command_id))
      browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

std::wstring BrowserView::GetWindowName() const {
  return browser_->GetWindowPlacementKey();
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      bool maximized,
                                      bool always_on_top) {
  // If fullscreen_ is true, we've just changed into fullscreen mode, and we're
  // catching the going-into-fullscreen sizing and positioning calls, which we
  // want to ignore.
  if (!fullscreen_ && browser_->ShouldSaveWindowPlacement()) {
    WindowDelegate::SaveWindowPlacement(bounds, maximized, always_on_top);
    browser_->SaveWindowPlacement(bounds, maximized);
  }
}

bool BrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  *bounds = browser_->GetSavedWindowBounds();
  if (browser_->type() & Browser::TYPE_POPUP) {
    // We are a popup window. The value passed in |bounds| represents two
    // pieces of information:
    // - the position of the window, in screen coordinates (outer position).
    // - the size of the content area (inner size).
    // We need to use these values to determine the appropriate size and
    // position of the resulting window.
    if (IsToolbarVisible()) {
      // If we're showing the toolbar, we need to adjust |*bounds| to include
      // its desired height, since the toolbar is considered part of the
      // window's client area as far as GetWindowBoundsForClientBounds is
      // concerned...
      bounds->set_height(
          bounds->height() + toolbar_->GetPreferredSize().height());
    }

    gfx::Rect window_rect =
        frame_->GetNonClientView()->GetWindowBoundsForClientBounds(*bounds);
    window_rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (window_rect.x() == 0 && window_rect.y() == 0) {
      gfx::Size size = window_rect.size();
      window_rect.set_origin(WindowSizer::GetDefaultPopupOrigin(size));
    }

    *bounds = window_rect;
  }

  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred
  // size.
  return true;
}

bool BrowserView::GetSavedMaximizedState(bool* maximized) const {
  *maximized = browser_->GetSavedMaximizedState();
  return true;
}

views::View* BrowserView::GetContentsView() {
  return contents_container_;
}

views::ClientView* BrowserView::CreateClientView(views::Window* window) {
  set_window(window);
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

bool BrowserView::CanClose() const {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_->IsDragSessionActive())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->Hide();
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  NotificationService::current()->Notify(
      NotificationType::WINDOW_CLOSED,
      Source<HWND>(frame_->GetNativeView()),
      NotificationService::NoDetails());
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  if (CanCurrentlyResize()) {
    CRect client_rect;
    ::GetClientRect(frame_->GetNativeView(), &client_rect);
    gfx::Size resize_corner_size = ResizeCorner::GetSize();
    gfx::Rect resize_corner_rect(client_rect.right - resize_corner_size.width(),
        client_rect.bottom - resize_corner_size.height(),
        resize_corner_size.width(), resize_corner_size.height());
    bool rtl_dir = (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
    if (rtl_dir)
      resize_corner_rect.set_x(0);
    if (resize_corner_rect.Contains(point)) {
      if (rtl_dir)
        return HTBOTTOMLEFT;
      return HTBOTTOMRIGHT;
    }
  }

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (IsTabStripVisible()) {
    gfx::Point point_in_view_coords(point);
    View::ConvertPointToView(GetParent(), this, &point_in_view_coords);

    // See if the mouse pointer is within the bounds of the TabStrip.
    gfx::Point point_in_tabstrip_coords(point);
    View::ConvertPointToView(GetParent(), tabstrip_, &point_in_tabstrip_coords);
    if (tabstrip_->HitTest(point_in_tabstrip_coords)) {
      if (tabstrip_->PointIsWithinWindowCaption(point_in_tabstrip_coords))
        return HTCAPTION;
      return HTCLIENT;
    }

    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    if (!IsMaximized() &&
        (point_in_view_coords.y() < tabstrip_->y() + kTabShadowSize)) {
      // We return HTNOWHERE as this is a signal to our containing
      // NonClientView that it should figure out what the correct hit-test
      // code is given the mouse position...
      return HTNOWHERE;
    }
  }

  // If the point's y coordinate is below the top of the toolbar and otherwise
  // within the bounds of this view, the point is considered to be within the
  // client area.
  gfx::Rect bv_bounds = bounds();
  bv_bounds.Offset(0, toolbar_->y());
  bv_bounds.set_height(bv_bounds.height() - toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTCLIENT;

  // If the point's y coordinate is above the top of the toolbar, but not in
  // the tabstrip (per previous checking in this function), then we consider it
  // in the window caption (e.g. the area to the right of the tabstrip
  // underneath the window controls). However, note that we DO NOT return
  // HTCAPTION here, because when the window is maximized the window controls
  // will fall into this space (since the BrowserView is sized to entire size
  // of the window at that point), and the HTCAPTION value will cause the
  // window controls not to work. So we return HTNOWHERE so that the caller
  // will hit-test the window controls before finally falling back to
  // HTCAPTION.
  bv_bounds = bounds();
  bv_bounds.set_height(toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTNOWHERE;

  // If the point is somewhere else, delegate to the default implementation.
  return views::ClientView::NonClientHitTest(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

std::string BrowserView::GetClassName() const {
  return kBrowserViewClassName;
}

void BrowserView::Layout() {
  if (ignore_layout_)
    return;

  int top = LayoutTabStrip();
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutDownloadShelf();
  LayoutTabContents(top, bottom);
  // This must be done _after_ we lay out the TabContents since this code calls
  // back into us to find the bounding box the find bar must be laid out within,
  // and that code depends on the TabContentsContainer's bounds being up to
  // date.
  static_cast<FindBarWin*>(find_bar_controller_->get_find_bar())->
      MoveWindowIfNecessary(gfx::Rect(), true);
  LayoutStatusBubble(bottom);
#ifdef CHROME_PERSONALIZATION
  if (IsPersonalizationEnabled()) {
    Personalization::ConfigureFramePersonalization(personalization_,
                                                   toolbar_, 0);
  }
#endif
  SchedulePaint();
}

void BrowserView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this && GetWidget() && !initialized_) {
    Init();
    initialized_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, private:

void BrowserView::InitSystemMenu() {
  HMENU system_menu = GetSystemMenu(frame_->GetNativeView(), FALSE);
  system_menu_.reset(new Menu(system_menu));
  int insertion_index = std::max(0, system_menu_->ItemCount() - 1);
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (IsBrowserTypeNormal()) {
    system_menu_->AddSeparator(insertion_index);
    system_menu_->AddMenuItemWithLabel(insertion_index, IDC_TASK_MANAGER,
                                       l10n_util::GetString(IDS_TASK_MANAGER));
    // If it's a regular browser window with tabs, we don't add any more items,
    // since it already has menus (Page, Chrome).
  } else {
    BuildMenuForTabStriplessWindow(system_menu_.get(), insertion_index);
  }
}

int BrowserView::LayoutTabStrip() {
  gfx::Rect tabstrip_bounds = frame_->GetBoundsForTabStrip(tabstrip_);
  gfx::Point tabstrip_origin = tabstrip_bounds.origin();
  ConvertPointToView(GetParent(), this, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);
  bool visible = IsTabStripVisible();
  int y = visible ? tabstrip_bounds.y() : 0;
  int height = visible ? tabstrip_bounds.height() : 0;
  tabstrip_->SetVisible(visible);
  tabstrip_->SetBounds(tabstrip_bounds.x(), y, tabstrip_bounds.width(), height);
  return y + height;
}

int BrowserView::LayoutToolbar(int top) {
  int browser_view_width = width();
#ifdef CHROME_PERSONALIZATION
  if (IsPersonalizationEnabled())
    Personalization::AdjustBrowserView(personalization_, &browser_view_width);
#endif
  bool visible = IsToolbarVisible();
  toolbar_->GetLocationBarView()->SetFocusable(visible);
  int y = top -
      ((visible && IsTabStripVisible()) ? kToolbarTabStripVerticalOverlap : 0);
  int height = visible ? toolbar_->GetPreferredSize().height() : 0;
  toolbar_->SetVisible(visible);
  toolbar_->SetBounds(0, y, browser_view_width, height);
  return y + height;
}

int BrowserView::LayoutBookmarkAndInfoBars(int top) {
  find_bar_y_ = top + y() - 1;
  if (active_bookmark_bar_) {
    // If we're showing the Bookmark bar in detached style, then we need to show
    // any Info bar _above_ the Bookmark bar, since the Bookmark bar is styled
    // to look like it's part of the page.
    if (bookmark_bar_view_->IsDetachedStyle())
      return LayoutBookmarkBar(LayoutInfoBar(top));
    // Otherwise, Bookmark bar first, Info bar second.
    top = LayoutBookmarkBar(top);
  }
  find_bar_y_ = top + y() - 1;
  return LayoutInfoBar(top);
}

int BrowserView::LayoutBookmarkBar(int top) {
  DCHECK(active_bookmark_bar_);
  bool visible = IsBookmarkBarVisible();
  int height, y = top;
  if (visible) {
    y -= kSeparationLineHeight + (bookmark_bar_view_->IsDetachedStyle() ?
        0 : bookmark_bar_view_->GetToolbarOverlap());
    height = bookmark_bar_view_->GetPreferredSize().height();
  } else {
    height = 0;
  }
  bookmark_bar_view_->SetVisible(visible);
  bookmark_bar_view_->SetBounds(0, y, width(), height);
  return y + height;
}

int BrowserView::LayoutInfoBar(int top) {
  bool visible = browser_->SupportsWindowFeature(Browser::FEATURE_INFOBAR);
  int height = visible ? infobar_container_->GetPreferredSize().height() : 0;
  infobar_container_->SetVisible(visible);
  infobar_container_->SetBounds(0, top, width(), height);
  return top + height;
}

void BrowserView::LayoutTabContents(int top, int bottom) {
  contents_container_->SetBounds(0, top, width(), bottom - top);
}

int BrowserView::LayoutDownloadShelf() {
  int bottom = height();
  if (active_download_shelf_) {
    bool visible = browser_->SupportsWindowFeature(
        Browser::FEATURE_DOWNLOADSHELF);
    int height =
        visible ? active_download_shelf_->GetPreferredSize().height() : 0;
    active_download_shelf_->SetVisible(visible);
    active_download_shelf_->SetBounds(0, bottom - height, width(), height);
    active_download_shelf_->Layout();
    bottom -= height;
  }
  return bottom;
}

void BrowserView::LayoutStatusBubble(int top) {
  // In restored mode, the client area has a client edge between it and the
  // frame.
  int overlap = kStatusBubbleOverlap +
      (IsMaximized() ? 0 : views::NonClientFrameView::kClientEdgeThickness);
  gfx::Point origin(-overlap, top - kStatusBubbleHeight + overlap);
  ConvertPointToView(this, GetParent(), &origin);
  status_bubble_->SetBounds(origin.x(), origin.y(), width() / 3,
                            kStatusBubbleHeight);
}

bool BrowserView::MaybeShowBookmarkBar(TabContents* contents) {
  views::View* new_bookmark_bar_view = NULL;
  if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)
      && contents) {
    if (!bookmark_bar_view_.get()) {
      bookmark_bar_view_.reset(new BookmarkBarView(contents->profile(),
                                                   browser_.get()));
      bookmark_bar_view_->SetParentOwned(false);
    } else {
      bookmark_bar_view_->SetProfile(contents->profile());
    }
    bookmark_bar_view_->SetPageNavigator(contents);
    new_bookmark_bar_view = bookmark_bar_view_.get();
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view, &active_bookmark_bar_);
}

bool BrowserView::MaybeShowInfoBar(TabContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and TabContents and this
  //             view is sorted out.
  return true;
}

bool BrowserView::MaybeShowDownloadShelf(TabContents* contents) {
  views::View* new_shelf = NULL;
  if (contents && contents->IsDownloadShelfVisible()) {
    new_shelf = static_cast<DownloadShelfView*>(contents->GetDownloadShelf());
    if (new_shelf != active_download_shelf_)
      new_shelf->AddChildView(new ResizeCorner());
  }
  return UpdateChildViewAndLayout(new_shelf, &active_download_shelf_);
}

void BrowserView::UpdateUIForContents(TabContents* contents) {
  bool needs_layout = MaybeShowBookmarkBar(contents);
  needs_layout |= MaybeShowInfoBar(contents);
  needs_layout |= MaybeShowDownloadShelf(contents);
  if (needs_layout)
    Layout();
}

bool BrowserView::UpdateChildViewAndLayout(views::View* new_view,
                                           views::View** old_view) {
  DCHECK(old_view);
  if (*old_view == new_view) {
    // The views haven't changed, if the views pref changed schedule a layout.
    if (new_view) {
      if (new_view->GetPreferredSize().height() != new_view->height())
        return true;
    }
    return false;
  }

  // The views differ, and one may be null (but not both). Remove the old
  // view (if it non-null), and add the new one (if it is non-null). If the
  // height has changed, schedule a layout, otherwise reuse the existing
  // bounds to avoid scheduling a layout.

  int current_height = 0;
  if (*old_view) {
    current_height = (*old_view)->height();
    RemoveChildView(*old_view);
  }

  int new_height = 0;
  if (new_view) {
    new_height = new_view->GetPreferredSize().height();
    AddChildView(new_view);
  }
  bool changed = false;
  if (new_height != current_height) {
    changed = true;
  } else if (new_view && *old_view) {
    // The view changed, but the new view wants the same size, give it the
    // bounds of the last view and have it repaint.
    new_view->SetBounds((*old_view)->bounds());
    new_view->SchedulePaint();
  } else if (new_view) {
    DCHECK(new_height == 0);
    // The heights are the same, but the old view is null. This only happens
    // when the height is zero. Zero out the bounds.
    new_view->SetBounds(0, 0, 0, 0);
  }
  *old_view = new_view;
  return changed;
}

void BrowserView::LoadAccelerators() {
  HACCEL accelerator_table = AtlLoadAccelerators(IDR_MAINFRAME);
  DCHECK(accelerator_table);

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return;
  }

  ACCEL* accelerators = static_cast<ACCEL*>(malloc(sizeof(ACCEL) * count));
  CopyAcceleratorTable(accelerator_table, accelerators, count);

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's build our own accelerator table.
  accelerator_table_.reset(new std::map<views::Accelerator, int>);
  for (int i = 0; i < count; ++i) {
    bool alt_down = (accelerators[i].fVirt & FALT) == FALT;
    bool ctrl_down = (accelerators[i].fVirt & FCONTROL) == FCONTROL;
    bool shift_down = (accelerators[i].fVirt & FSHIFT) == FSHIFT;
    views::Accelerator accelerator(accelerators[i].key, shift_down, ctrl_down,
                                   alt_down);
    (*accelerator_table_)[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }

  // We don't need the Windows accelerator table anymore.
  free(accelerators);
}

void BrowserView::BuildMenuForTabStriplessWindow(Menu* menu,
                                                 int insertion_index) {
  encoding_menu_delegate_.reset(new EncodingMenuControllerDelegate(
      browser_.get()));

  for (int i = 0; i < arraysize(kMenuLayout); ++i) {
    if (kMenuLayout[i].separator) {
      menu->AddSeparator(insertion_index);
    } else {
      int command = kMenuLayout[i].command;
      if (command == IDC_ENCODING_MENU) {
        Menu* encoding_menu = menu->AddSubMenu(
            insertion_index,
            IDC_ENCODING_MENU,
            l10n_util::GetString(IDS_ENCODING_MENU));
        encoding_menu->set_delegate(encoding_menu_delegate_.get());
        EncodingMenuControllerDelegate::BuildEncodingMenu(browser_->profile(),
                                                          encoding_menu);
      } else if (command == IDC_ZOOM_MENU) {
        Menu* zoom_menu = menu->AddSubMenu(insertion_index, IDC_ZOOM_MENU,
                                           l10n_util::GetString(IDS_ZOOM_MENU));
        zoom_menu->AppendMenuItemWithLabel(
            IDC_ZOOM_PLUS,
            l10n_util::GetString(IDS_ZOOM_PLUS));
        zoom_menu->AppendMenuItemWithLabel(
            IDC_ZOOM_NORMAL,
            l10n_util::GetString(IDS_ZOOM_NORMAL));
        zoom_menu->AppendMenuItemWithLabel(
            IDC_ZOOM_MINUS,
            l10n_util::GetString(IDS_ZOOM_MINUS));
      } else {
        menu->AddMenuItemWithLabel(insertion_index, command,
                                   l10n_util::GetString(kMenuLayout[i].label));
      }
    }
  }
}

int BrowserView::GetCommandIDForAppCommandID(int app_command_id) const {
  switch (app_command_id) {
    // NOTE: The order here matches the APPCOMMAND declaration order in the
    // Windows headers.
    case APPCOMMAND_BROWSER_BACKWARD: return IDC_BACK;
    case APPCOMMAND_BROWSER_FORWARD:  return IDC_FORWARD;
    case APPCOMMAND_BROWSER_REFRESH:  return IDC_RELOAD;
    case APPCOMMAND_BROWSER_HOME:     return IDC_HOME;
    case APPCOMMAND_BROWSER_STOP:     return IDC_STOP;
    case APPCOMMAND_BROWSER_SEARCH:   return IDC_FOCUS_SEARCH;
    case APPCOMMAND_HELP:             return IDC_HELP_PAGE;
    case APPCOMMAND_NEW:              return IDC_NEW_TAB;
    case APPCOMMAND_OPEN:             return IDC_OPEN_FILE;
    case APPCOMMAND_CLOSE:            return IDC_CLOSE_TAB;
    case APPCOMMAND_SAVE:             return IDC_SAVE_PAGE;
    case APPCOMMAND_PRINT:            return IDC_PRINT;
    case APPCOMMAND_COPY:             return IDC_COPY;
    case APPCOMMAND_CUT:              return IDC_CUT;
    case APPCOMMAND_PASTE:            return IDC_PASTE;

      // TODO(pkasting): http://b/1113069 Handle these.
    case APPCOMMAND_UNDO:
    case APPCOMMAND_REDO:
    case APPCOMMAND_SPELL_CHECK:
    default:                          return -1;
  }
}

void BrowserView::LoadingAnimationCallback() {
  if (browser_->type() == Browser::TYPE_NORMAL) {
    // Loading animations are shown in the tab for tabbed windows.  We check the
    // browser type instead of calling IsTabStripVisible() because the latter
    // will return false for fullscreen windows, but we still need to update
    // their animations (so that when they come out of fullscreen mode they'll
    // be correct).
    tabstrip_->UpdateLoadingAnimations();
  } else if (ShouldShowWindowIcon()) {
    // ... or in the window icon area for popups and app windows.
    TabContents* tab_contents = browser_->GetSelectedTabContents();
    // GetSelectedTabContents can return NULL for example under Purify when
    // the animations are running slowly and this function is called on a timer
    // through LoadingAnimationCallback.
    frame_->UpdateThrobber(tab_contents && tab_contents->is_loading());
  }
}

void BrowserView::InitHangMonitor() {
  PrefService* pref_service = g_browser_process->local_state();
  if (!pref_service)
    return;

  int plugin_message_response_timeout =
      pref_service->GetInteger(prefs::kPluginMessageResponseTimeout);
  int hung_plugin_detect_freq =
      pref_service->GetInteger(prefs::kHungPluginDetectFrequency);
  if ((hung_plugin_detect_freq > 0) &&
      hung_window_detector_.Initialize(GetWidget()->GetNativeView(),
                                       plugin_message_response_timeout)) {
    ticker_.set_tick_interval(hung_plugin_detect_freq);
    ticker_.RegisterTickHandler(&hung_window_detector_);
    ticker_.Start();

    pref_service->SetInteger(prefs::kPluginMessageResponseTimeout,
                             plugin_message_response_timeout);
    pref_service->SetInteger(prefs::kHungPluginDetectFrequency,
                             hung_plugin_detect_freq);
  }
}

// static
void BrowserView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    initialized = true;
  }
}

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  BrowserView* browser_view = new BrowserView(browser);
  (new BrowserFrame(browser_view))->Init();
  return browser_view;
}
