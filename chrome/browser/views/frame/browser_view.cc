// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include "chrome/browser/views/frame/browser_view.h"

#include "base/file_version_info.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/encoding_menu_controller_delegate.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/about_chrome_view.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/bookmark_manager_view.h"
#include "chrome/browser/views/bug_report_view.h"
#include "chrome/browser/views/clear_browsing_data.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/html_dialog_view.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/infobars/infobar_container.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/hwnd_notification_source.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"

#include "chromium_strings.h"
#include "generated_resources.h"

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
// The distance of the status bubble from the left edge of the window.
static const int kStatusBubbleHorizontalOffset = 3;
// The distance of the status bubble from the bottom edge of the window.
static const int kStatusBubbleVerticalOffset = 2;
// An offset distance between certain toolbars and the toolbar that preceded
// them in layout.
static const int kSeparationLineHeight = 1;
// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";
// The distance between tiled windows.
static const int kWindowTilePixels = 10;
// How frequently we check for hung plugin windows.
static const int kDefaultHungPluginDetectFrequency = 2000;
// How long do we wait before we consider a window hung (in ms).
static const int kDefaultPluginMessageResponseTimeout = 30000;
// The number of milliseconds between loading animation frames.
static const int kLoadingAnimationFrameTimeMs = 30;

static const struct { bool separator; int command; int label; } kMenuLayout[] = {
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
// BrowserView, public:

BrowserView::BrowserView(Browser* browser)
    : ClientView(NULL, NULL),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      active_info_bar_(NULL),
      active_download_shelf_(NULL),
      toolbar_(NULL),
      contents_container_(NULL),
      initialized_(false),
      can_drop_(false),
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0),
#ifdef CHROME_PERSONALIZATION
      personalization_enabled_(false),
      personalization_(NULL),
#endif
      forwarding_to_tab_strip_(false) {
  InitClass();
  show_bookmark_bar_pref_.Init(prefs::kShowBookmarkBar,
                               browser_->profile()->GetPrefs(), this);
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserView::~BrowserView() {
  browser_->tabstrip_model()->RemoveObserver(this);

  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);
}

// static
BrowserWindow* BrowserView::GetBrowserWindowForHWND(HWND window) {
  if (IsWindow(window)) {
    HANDLE data = GetProp(window, kBrowserWindowKey);
    if (data)
      return reinterpret_cast<BrowserWindow*>(data);
  }
  return NULL;
}

int BrowserView::GetShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  // When launched from bash, for some reason si.wShowWindow is set to SW_HIDE,
  // so we need to correct that condition.
  return si.wShowWindow == SW_HIDE ? SW_SHOWNORMAL : si.wShowWindow;
}

void BrowserView::WindowMoved() {
  // Cancel any tabstrip animations, some of them may be invalidated by the
  // window being repositioned.
  // Comment out for one cycle to see if this fixes dist tests.
  //tabstrip_->DestroyDragController();

  status_bubble_->Reposition();

  // Close the omnibox popup, if any.
  if (GetLocationBarView())
    GetLocationBarView()->location_entry()->ClosePopup();
}

gfx::Rect BrowserView::GetToolbarBounds() const {
  return toolbar_->bounds();
}

gfx::Rect BrowserView::GetClientAreaBounds() const {
  gfx::Rect container_bounds = contents_container_->bounds();
  container_bounds.Offset(x(), y());
  return container_bounds;
}

int BrowserView::GetTabStripHeight() const {
  return tabstrip_->GetPreferredHeight();
}

bool BrowserView::IsToolbarVisible() const {
  return SupportsWindowFeature(FEATURE_TOOLBAR) ||
         SupportsWindowFeature(FEATURE_LOCATIONBAR);
}

bool BrowserView::IsTabStripVisible() const {
  return SupportsWindowFeature(FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::ShouldShowOffTheRecordAvatar() const {
  return IsOffTheRecord() && browser_->type() == Browser::TYPE_NORMAL;
}

bool BrowserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  DCHECK(accelerator_table_.get());
  std::map<views::Accelerator, int>::const_iterator iter =
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

bool BrowserView::SystemCommandReceived(UINT notification_code,
                                        const gfx::Point& point) {
  bool handled = false;

  if (browser_->SupportsCommand(notification_code)) {
    browser_->ExecuteCommand(notification_code);
    handled = true;
  }

  return handled;
}

void BrowserView::AddViewToDropList(views::View* view) {
  dropable_views_.insert(view);
}

bool BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  if (BrowserList::IsShowingAppModalDialog()) {
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
      system_menu_->EnableMenuItemByID(command,
                                       browser_->IsCommandEnabled(command));
    }
  }
}

bool BrowserView::SupportsWindowFeature(WindowFeature feature) const {
  return !!(FeaturesForBrowserType(browser_->type()) & feature);
}

// static
unsigned int BrowserView::FeaturesForBrowserType(Browser::Type type) {
  unsigned int features = FEATURE_INFOBAR | FEATURE_DOWNLOADSHELF;
  if (type == Browser::TYPE_NORMAL)
    features |= FEATURE_TABSTRIP | FEATURE_TOOLBAR | FEATURE_BOOKMARKBAR;
  if (type != Browser::TYPE_APP)
    features |= FEATURE_LOCATIONBAR;
  if (type != Browser::TYPE_NORMAL)
    features |= FEATURE_TITLEBAR;
  return features;
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
  SetProp(GetWidget()->GetHWND(), kBrowserWindowKey, this);

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!CommandLine().HasSwitch(switches::kDisableHangMonitor))
    InitHangMonitor();

  LoadAccelerators();
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  tabstrip_ = new TabStrip(browser_->tabstrip_model());
  tabstrip_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  AddChildView(tabstrip_);

  toolbar_ = new BrowserToolbarView(browser_->controller(), browser_.get());
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  infobar_container_ = new InfoBarContainer(this);
  AddChildView(infobar_container_);

  contents_container_ = new TabContentsContainerView;
  set_contents_view(contents_container_);
  AddChildView(contents_container_);

  status_bubble_.reset(new StatusBubble(GetWidget()));

#ifdef CHROME_PERSONALIZATION
  EnablePersonalization(CommandLine().HasSwitch(switches::kEnableP13n));
  if (IsPersonalizationEnabled()) {
    personalization_ = Personalization::CreateFramePersonalization(
        browser_->profile(), this);
  }
#endif

  InitSystemMenu();
}

void BrowserView::Show() {
  // If the window is already visible, just activate it.
  if (frame_->GetWindow()->IsVisible()) {
    frame_->GetWindow()->Activate();
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
  if (selected_tab_contents)
    selected_tab_contents->RestoreFocus();

  frame_->GetWindow()->Show();
  int show_state = frame_->GetWindow()->GetShowState();
  if (show_state == SW_SHOWNORMAL || show_state == SW_SHOWMAXIMIZED)
    frame_->GetWindow()->Activate();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  frame_->GetWindow()->SetBounds(bounds);
}

void BrowserView::Close() {
  frame_->GetWindow()->Close();
}

void BrowserView::Activate() {
  frame_->GetWindow()->Activate();
}

void BrowserView::FlashFrame() {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = frame_->GetWindow()->GetHWND();
  fwi.dwFlags = FLASHW_ALL;
  fwi.uCount = 4;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);
}

void* BrowserView::GetNativeHandle() {
  return GetWidget()->GetHWND();
}

TabStrip* BrowserView::GetTabStrip() const {
  return tabstrip_;
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
  frame_->GetWindow()->UpdateWindowTitle();
  if (ShouldShowWindowIcon())
    frame_->GetWindow()->UpdateWindowIcon();
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

gfx::Rect BrowserView::GetNormalBounds() const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool ret = !!GetWindowPlacement(frame_->GetWindow()->GetHWND(), &wp);
  DCHECK(ret);
  return gfx::Rect(wp.rcNormalPosition);
}

bool BrowserView::IsMaximized() {
  return frame_->GetWindow()->IsMaximized();
}

ToolbarStarToggle* BrowserView::GetStarButton() const {
  return toolbar_->star_button();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_->GetLocationBarView();
}

GoButton* BrowserView::GetGoButton() const {
  return toolbar_->GetGoButton();
}

BookmarkBarView* BrowserView::GetBookmarkBarView() {
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!bookmark_bar_view_.get()) {
    bookmark_bar_view_.reset(new BookmarkBarView(current_tab->profile(),
                                                 browser_.get()));
    bookmark_bar_view_->SetParentOwned(false);
  } else {
    bookmark_bar_view_->SetProfile(current_tab->profile());
  }
  bookmark_bar_view_->SetPageNavigator(current_tab);
  return bookmark_bar_view_.get();
}

BrowserView* BrowserView::GetBrowserView() const {
  return NULL;
}

void BrowserView::UpdateToolbar(TabContents* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView::FocusToolbar() {
  // Do not restore the button that previously had accessibility focus, if
  // focus is set by using the toolbar focus keyboard shortcut.
  toolbar_->set_acc_focused_view(NULL);
  toolbar_->RequestFocus();
}

void BrowserView::DestroyBrowser() {
  // Explicitly delete the BookmarkBarView now. That way we don't have to
  // worry about the BookmarkBarView potentially outliving the Browser &
  // Profile.
  bookmark_bar_view_.reset();
  browser_.reset();
}

bool BrowserView::IsBookmarkBarVisible() const {
  if (!bookmark_bar_view_.get())
    return false;

  if (bookmark_bar_view_->OnNewTabPage() || bookmark_bar_view_->IsAnimating())
    return true;

  // 1 is the minimum in GetPreferredSize for the bookmark bar.
  return bookmark_bar_view_->GetPreferredSize().height() > 1;
}

void BrowserView::ToggleBookmarkBar() {
  BookmarkBarView::ToggleWhenVisible(browser_->profile());
}

void BrowserView::ShowAboutChromeDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetHWND(), gfx::Rect(),
      new AboutChromeView(browser_->profile()))->Show();
}

void BrowserView::ShowBookmarkManager() {
  BookmarkManagerView::Show(browser_->profile());
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
  win_util::GrabWindowSnapshot(GetWidget()->GetHWND(), screenshot_png);
  // the BugReportView takes ownership of the png data, and will dispose of
  // it in its destructor.
  bug_report_view->set_png_data(screenshot_png);

  // Create and show the dialog
  views::Window::CreateChromeWindow(GetWidget()->GetHWND(), gfx::Rect(),
                                    bug_report_view)->Show();
}

void BrowserView::ShowClearBrowsingDataDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetHWND(), gfx::Rect(),
      new ClearBrowsingDataView(browser_->profile()))->Show();
}

void BrowserView::ShowImportDialog() {
  views::Window::CreateChromeWindow(
      GetWidget()->GetHWND(), gfx::Rect(),
      new ImporterView(browser_->profile()))->Show();
}

void BrowserView::ShowSearchEnginesDialog() {
  KeywordEditorView::Show(browser_->profile());
}

void BrowserView::ShowPasswordManager() {
  PasswordManagerView::Show(browser_->profile());
}

void BrowserView::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                 void* parent_window) {
  HWND parent_hwnd = reinterpret_cast<HWND>(parent_window);
  parent_hwnd = parent_hwnd ? parent_hwnd : GetWidget()->GetHWND();
  HtmlDialogView* html_view = new HtmlDialogView(browser_.get(),
                                                 browser_->profile(),
                                                 delegate);
  views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), html_view);
  html_view->InitDialog();
  html_view->window()->Show();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, NotificationObserver implementation:

void BrowserView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NOTIFY_PREF_CHANGED &&
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
  if (old_contents && !old_contents->is_being_destroyed())
    old_contents->StoreFocus();

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
      !browser_->tabstrip_model()->closing_all()) {
    new_contents->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);
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

views::View* BrowserView::GetInitiallyFocusedView() const {
  return GetLocationBarView();
}

bool BrowserView::ShouldShowWindowTitle() const {
  return SupportsWindowFeature(FEATURE_TITLEBAR);
}

SkBitmap BrowserView::GetWindowIcon() {
  if (browser_->type() == Browser::TYPE_APP)
    return browser_->GetCurrentPageIcon();
  return SkBitmap();
}

bool BrowserView::ShouldShowWindowIcon() const {
  return SupportsWindowFeature(FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  if (browser_->SupportsCommand(command_id)) {
    if (browser_->IsCommandEnabled(command_id))
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
  if (browser_->ShouldSaveWindowPlacement()) {
    WindowDelegate::SaveWindowPlacement(bounds, maximized, always_on_top);
    browser_->SaveWindowPlacement(bounds, maximized);
  }
}

bool BrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  *bounds = browser_->GetSavedWindowBounds();
  if (browser_->type() == Browser::TYPE_POPUP) {
    // We are a popup window. The value passed in |bounds| represents two
    // pieces of information:
    // - the position of the window, in screen coordinates (outer position).
    // - the size of the content area (inner size).
    // We need to use these values to determine the appropriate size and
    // position of the resulting window.
    if (SupportsWindowFeature(FEATURE_TOOLBAR) ||
        SupportsWindowFeature(FEATURE_LOCATIONBAR)) {
      // If we're showing the toolbar, we need to adjust |*bounds| to include
      // its desired height, since the toolbar is considered part of the
      // window's client area as far as GetWindowBoundsForClientBounds is
      // concerned...
      bounds->set_height(
          bounds->height() + toolbar_->GetPreferredSize().height());
    }

    gfx::Rect window_rect = frame_->GetWindowBoundsForClientBounds(*bounds);
    window_rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (window_rect.x() == 0 && window_rect.y() == 0) {
      window_rect.set_origin(GetNormalBounds().origin());
      window_rect.Offset(kWindowTilePixels, kWindowTilePixels);
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

void BrowserView::WindowClosing() {
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
    frame_->GetWindow()->Hide();
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  NotificationService::current()->Notify(
      NOTIFY_WINDOW_CLOSED, Source<HWND>(frame_->GetWindow()->GetHWND()),
      NotificationService::NoDetails());
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip, or the TabStrip could be
  // animating.
  if (IsTabStripVisible() && tabstrip_->CanProcessInputEvents()) {
    views::Window* window = frame_->GetWindow();
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
    if (!window->IsMaximized() &&
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
  return ClientView::NonClientHitTest(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

void BrowserView::Layout() {
  int top = LayoutTabStrip();
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutDownloadShelf();
  LayoutTabContents(top, bottom);
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
  if (!is_add)
    dropable_views_.erase(child);
}

bool BrowserView::CanDrop(const OSExchangeData& data) {
  can_drop_ = (tabstrip_->IsVisible() && !tabstrip_->IsAnimating() &&
               data.HasURL());
  return can_drop_;
}

void BrowserView::OnDragEntered(const views::DropTargetEvent& event) {
  if (can_drop_ && ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<views::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event));
    tabstrip_->OnDragEntered(*mapped_event.get());
  }
}

int BrowserView::OnDragUpdated(const views::DropTargetEvent& event) {
  if (can_drop_) {
    if (ShouldForwardToTabStrip(event)) {
      scoped_ptr<views::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
      if (!forwarding_to_tab_strip_) {
        tabstrip_->OnDragEntered(*mapped_event.get());
        forwarding_to_tab_strip_ = true;
      }
      return tabstrip_->OnDragUpdated(*mapped_event.get());
    } else if (forwarding_to_tab_strip_) {
      forwarding_to_tab_strip_ = false;
      tabstrip_->OnDragExited();
    }
  }
  return DragDropTypes::DRAG_NONE;
}

void BrowserView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip_->OnDragExited();
  }
}

int BrowserView::OnPerformDrop(const views::DropTargetEvent& event) {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    scoped_ptr<views::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
    return tabstrip_->OnPerformDrop(*mapped_event.get());
  }
  return DragDropTypes::DRAG_NONE;
}


///////////////////////////////////////////////////////////////////////////////
// BrowserView, private:

void BrowserView::InitSystemMenu() {
  HMENU system_menu = GetSystemMenu(frame_->GetWindow()->GetHWND(), FALSE);
  system_menu_.reset(new Menu(system_menu));
  int insertion_index = std::max(0, system_menu_->ItemCount() - 1);
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (browser_->type() == Browser::TYPE_NORMAL) {
    system_menu_->AddSeparator(insertion_index);
    system_menu_->AddMenuItemWithLabel(insertion_index, IDC_TASK_MANAGER,
                                       l10n_util::GetString(IDS_TASK_MANAGER));
    // If it's a regular browser window with tabs, we don't add any more items,
    // since it already has menus (Page, Chrome).
  } else {
    BuildMenuForTabStriplessWindow(system_menu_.get(), insertion_index);
  }
}

bool BrowserView::ShouldForwardToTabStrip(
    const views::DropTargetEvent& event) {
  if (!tabstrip_->IsVisible())
    return false;

  const int tab_y = tabstrip_->y();
  const int tab_height = tabstrip_->height();
  if (event.y() >= tab_y + tab_height)
    return false;

  if (event.y() >= tab_y)
    return true;

  // Mouse isn't over the tab strip. Only forward if the mouse isn't over
  // another view on the tab strip or is over a view we were told the user can
  // drop on.
  views::View* view_over_mouse = GetViewForPoint(event.location());
  return (view_over_mouse == this || view_over_mouse == tabstrip_ ||
          dropable_views_.find(view_over_mouse) != dropable_views_.end());
}

views::DropTargetEvent* BrowserView::MapEventToTabStrip(
    const views::DropTargetEvent& event) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, tabstrip_, &tab_strip_loc);
  return new views::DropTargetEvent(event.GetData(), tab_strip_loc.x(),
                                    tab_strip_loc.y(),
                                    event.GetSourceOperations());
}

int BrowserView::LayoutTabStrip() {
  if (IsTabStripVisible()) {
    gfx::Rect tabstrip_bounds = frame_->GetBoundsForTabStrip(tabstrip_);
    gfx::Point tabstrip_origin = tabstrip_bounds.origin();
    ConvertPointToView(GetParent(), this, &tabstrip_origin);
    tabstrip_bounds.set_origin(tabstrip_origin);
    tabstrip_->SetBounds(tabstrip_bounds.x(), tabstrip_bounds.y(),
                         tabstrip_bounds.width(), tabstrip_bounds.height());
    return tabstrip_bounds.bottom();
  }
  return 0;
}

int BrowserView::LayoutToolbar(int top) {
  if (IsToolbarVisible()) {
    gfx::Size ps = toolbar_->GetPreferredSize();
    int toolbar_y =
        top - (IsTabStripVisible() ? kToolbarTabStripVerticalOverlap : 0);
    // With detached popup windows with the aero glass frame, we need to offset
    // by a pixel to make things look good.
    if (!IsTabStripVisible() && win_util::ShouldUseVistaFrame())
      ps.Enlarge(0, -1);
    int browser_view_width = width();
#ifdef CHROME_PERSONALIZATION
    if (IsPersonalizationEnabled())
      Personalization::AdjustBrowserView(personalization_, &browser_view_width);
#endif
    toolbar_->SetBounds(0, toolbar_y, browser_view_width, ps.height());
    return toolbar_y + ps.height();
  }
  toolbar_->SetVisible(false);
  return top;
}

int BrowserView::LayoutBookmarkAndInfoBars(int top) {
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR)) {
    // If we have an Info-bar showing, and we're showing the New Tab Page, and
    // the Bookmark bar isn't visible on all tabs, then we need to show the
    // Info bar _above_ the Bookmark bar, since the Bookmark bar is styled to
    // look like it's part of the New Tab Page...
    if (active_bookmark_bar_ &&
        bookmark_bar_view_->OnNewTabPage() &&
        !bookmark_bar_view_->IsAlwaysShown()) {
      top = LayoutInfoBar(top);
      return LayoutBookmarkBar(top);
    }

    // If we're showing a regular bookmark bar and it's not below an infobar, 
    // make it overlap the toolbar so that the bar items can be drawn higher.
    if (active_bookmark_bar_)
      top -= bookmark_bar_view_->GetToolbarOverlap();

    // Otherwise, Bookmark bar first, Info bar second.
    top = LayoutBookmarkBar(top);
  }
  return LayoutInfoBar(top);
}

int BrowserView::LayoutBookmarkBar(int top) {
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR) && active_bookmark_bar_) {
    gfx::Size ps = active_bookmark_bar_->GetPreferredSize();
    if (!active_info_bar_ || show_bookmark_bar_pref_.GetValue())
      top -= kSeparationLineHeight;
    active_bookmark_bar_->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
  return top;
}

int BrowserView::LayoutInfoBar(int top) {
  if (SupportsWindowFeature(FEATURE_INFOBAR)) {
    // Layout the InfoBar container.
    gfx::Size ps = infobar_container_->GetPreferredSize();
    infobar_container_->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
  return top;
}

void BrowserView::LayoutTabContents(int top, int bottom) {
  contents_container_->SetBounds(0, top, width(), bottom - top);
}

int BrowserView::LayoutDownloadShelf() {
  int bottom = height();
  if (SupportsWindowFeature(FEATURE_DOWNLOADSHELF) && active_download_shelf_) {
    gfx::Size ps = active_download_shelf_->GetPreferredSize();
    active_download_shelf_->SetBounds(0, bottom - ps.height(), width(),
                                      ps.height());
    active_download_shelf_->Layout();
    bottom -= ps.height();
  }
  return bottom;
}

void BrowserView::LayoutStatusBubble(int top) {
  int status_bubble_y =
      top - kStatusBubbleHeight + kStatusBubbleVerticalOffset + y();
  status_bubble_->SetBounds(kStatusBubbleHorizontalOffset, status_bubble_y,
                            width() / 3, kStatusBubbleHeight);
}

bool BrowserView::MaybeShowBookmarkBar(TabContents* contents) {
  views::View* new_bookmark_bar_view = NULL;
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR) && contents) {
    new_bookmark_bar_view = GetBookmarkBarView();
    if (!show_bookmark_bar_pref_.GetValue() &&
        new_bookmark_bar_view->GetPreferredSize().height() == 0) {
      new_bookmark_bar_view = NULL;
    }
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view,
                                  &active_bookmark_bar_);
}

bool BrowserView::MaybeShowInfoBar(TabContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and TabContents and this
  //             view is sorted out.
  return true;
}

bool BrowserView::MaybeShowDownloadShelf(TabContents* contents) {
  views::View* new_shelf = NULL;
  if (contents && contents->IsDownloadShelfVisible())
    new_shelf = contents->GetDownloadShelfView();
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

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetWidget()->GetHWND());
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
      browser_.get(),
      browser_->controller()));

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
  if (SupportsWindowFeature(FEATURE_TABSTRIP)) {
    // Loading animations are shown in the tab for tabbed windows.
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
      hung_window_detector_.Initialize(GetWidget()->GetHWND(),
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
