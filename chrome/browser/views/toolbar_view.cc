// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/toolbar_view.h"

#include <string>

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/os_exchange_data.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/back_forward_menu_model_views.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/views/bookmark_menu_button.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "views/background.h"
#include "views/controls/button/button_dropdown.h"
#include "views/controls/label.h"
#if defined(OS_WIN)
#include "views/drag_utils.h"
#include "views/widget/tooltip_manager.h"
#endif
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kControlHorizOffset = 4;
static const int kControlVertOffset = 6;
static const int kControlIndent = 3;
static const int kStatusBubbleWidth = 480;

// Separation between the location bar and the menus.
static const int kMenuButtonOffset = 3;

// Padding to the right of the location bar
static const int kPaddingRight = 2;

static const int kPopupTopSpacingNonGlass = 3;
static const int kPopupBottomSpacingNonGlass = 2;
static const int kPopupBottomSpacingGlass = 1;

// The vertical distance between the bottom of the omnibox and the top of the
// popup.
static const int kOmniboxPopupVerticalSpacing = 2;
// The number of pixels of margin on the buttons on either side of the omnibox.
// We use this value to inset the bounds returned for the omnibox popup, since
// we want the popup to be only as wide as the visible frame of the omnibox.
static const int kOmniboxButtonsHorizontalMargin = 2;

static SkBitmap* kPopupBackgroundEdge = NULL;

BrowserToolbarView::BrowserToolbarView(Browser* browser)
    : EncodingMenuControllerDelegate(browser),
      model_(browser->toolbar_model()),
      acc_focused_view_(NULL),
      back_(NULL),
      forward_(NULL),
      reload_(NULL),
      home_(NULL),
      star_(NULL),
      location_bar_(NULL),
      go_(NULL),
      page_menu_(NULL),
      app_menu_(NULL),
      bookmark_menu_(NULL),
      profile_(NULL),
      browser_(browser),
      tab_(NULL),
      profiles_menu_(NULL),
      profiles_helper_(new GetProfilesHelper(this)) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);
  back_menu_model_.reset(new BackForwardMenuModelViews(
      browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  forward_menu_model_.reset(new BackForwardMenuModelViews(
      browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
  if (browser->type() == Browser::TYPE_NORMAL)
    display_mode_ = DISPLAYMODE_NORMAL;
  else
    display_mode_ = DISPLAYMODE_LOCATION;

  if (!kPopupBackgroundEdge) {
    kPopupBackgroundEdge = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_LOCATIONBG_POPUPMODE_EDGE);
  }
}

BrowserToolbarView::~BrowserToolbarView() {
  profiles_helper_->OnDelegateDeleted();
}

void BrowserToolbarView::Init(Profile* profile) {
  // Create all the individual Views in the Toolbar.
  CreateLeftSideControls();
  CreateCenterStack(profile);
  CreateRightSideControls(profile);

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void BrowserToolbarView::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  location_bar_->SetProfile(profile);
}

void BrowserToolbarView::CreateLeftSideControls() {
  back_ = new views::ButtonDropDown(this, back_menu_model_.get());
  back_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                  views::Event::EF_MIDDLE_BUTTON_DOWN);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetID(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ButtonDropDown(this, forward_menu_model_.get());
  forward_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                        views::Event::EF_MIDDLE_BUTTON_DOWN);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetID(VIEW_ID_FORWARD_BUTTON);

  reload_ = new views::ImageButton(this);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);

  home_ = new views::ImageButton(this);
  home_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                  views::Event::EF_MIDDLE_BUTTON_DOWN);
  home_->set_tag(IDC_HOME);
  home_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_HOME));
  home_->SetID(VIEW_ID_HOME_BUTTON);

  LoadLeftSideControlsImages();

  AddChildView(back_);
  AddChildView(forward_);
  AddChildView(reload_);
  AddChildView(home_);
}

void BrowserToolbarView::CreateCenterStack(Profile *profile) {
  star_ = new ToolbarStarToggle(this, this);
  star_->set_tag(IDC_STAR);
  star_->SetDragController(this);
  star_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_STAR));
  star_->SetToggledTooltipText(l10n_util::GetString(IDS_TOOLTIP_STARRED));
  star_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_STAR));
  star_->SetID(VIEW_ID_STAR_BUTTON);
  AddChildView(star_);

  location_bar_ = new LocationBarView(profile, browser_->command_updater(),
                                      model_, this,
                                      display_mode_ == DISPLAYMODE_LOCATION,
                                      this);

  // The Go button.
  go_ = new GoButton(location_bar_, browser_);
  go_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_GO));
  go_->SetID(VIEW_ID_GO_BUTTON);

  LoadCenterStackImages();

  AddChildView(location_bar_);
  location_bar_->Init();
  AddChildView(go_);
}

void BrowserToolbarView::CreateRightSideControls(Profile* profile) {
  page_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  page_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_PAGE));
  page_menu_->SetTooltipText(l10n_util::GetString(IDS_PAGEMENU_TOOLTIP));
  page_menu_->SetID(VIEW_ID_PAGE_MENU);


  app_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  app_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringF(IDS_APPMENU_TOOLTIP,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  app_menu_->SetID(VIEW_ID_APP_MENU);

  LoadRightSideControlsImages();

  AddChildView(page_menu_);
  AddChildView(app_menu_);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kBookmarkMenu)) {
    bookmark_menu_ = new BookmarkMenuButton(browser_);
    AddChildView(bookmark_menu_);
  } else {
    bookmark_menu_ = NULL;
  }
}

void BrowserToolbarView::LoadLeftSideControlsImages() {
  ThemeProvider* tp = GetThemeProvider();

  back_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_BACK));
  back_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_BACK_H));
  back_->SetImage(views::CustomButton::BS_PUSHED,
    tp->GetBitmapNamed(IDR_BACK_P));
  back_->SetImage(views::CustomButton::BS_DISABLED,
    tp->GetBitmapNamed(IDR_BACK_D));

  forward_->SetImage(views::CustomButton::BS_NORMAL,
    tp->GetBitmapNamed(IDR_FORWARD));
  forward_->SetImage(views::CustomButton::BS_HOT,
    tp->GetBitmapNamed(IDR_FORWARD_H));
  forward_->SetImage(views::CustomButton::BS_PUSHED,
    tp->GetBitmapNamed(IDR_FORWARD_P));
  forward_->SetImage(views::CustomButton::BS_DISABLED,
    tp->GetBitmapNamed(IDR_FORWARD_D));

  reload_->SetImage(views::CustomButton::BS_NORMAL,
    tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
    tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
    tp->GetBitmapNamed(IDR_RELOAD_P));

  home_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_HOME));
  home_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_HOME_H));
  home_->SetImage(views::CustomButton::BS_PUSHED,
    tp->GetBitmapNamed(IDR_HOME_P));
}

void BrowserToolbarView::LoadCenterStackImages() {
  ThemeProvider* tp = GetThemeProvider();

  star_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_STAR));
  star_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_STAR_H));
  star_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STAR_P));
  star_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_STAR_D));
  star_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STARRED));
  star_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STARRED_H));
  star_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STARRED_P));

  go_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_GO));
  go_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_GO_H));
  go_->SetImage(views::CustomButton::BS_PUSHED, tp->GetBitmapNamed(IDR_GO_P));
  go_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STOP));
  go_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STOP_H));
  go_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STOP_P));
}

void BrowserToolbarView::LoadRightSideControlsImages() {
  ThemeProvider* tp = GetThemeProvider();

  // We use different menu button images if the locale is right-to-left.
  if (UILayoutIsRightToLeft())
    page_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_PAGE_RTL));
  else
    page_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_PAGE));
  if (UILayoutIsRightToLeft())
    app_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_CHROME_RTL));
  else
    app_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_CHROME));
}

void BrowserToolbarView::Update(TabContents* tab, bool should_restore_state) {
  tab_ = tab;

  if (!location_bar_)
    return;

  location_bar_->Update(should_restore_state ? tab : NULL);
}

void BrowserToolbarView::OnInputInProgress(bool in_progress) {
  // The edit should make sure we're only notified when something changes.
  DCHECK(model_->input_in_progress() != in_progress);

  model_->set_input_in_progress(in_progress);
  location_bar_->Update(NULL);
}

void BrowserToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (back_ == NULL)
    return;

  if (!IsDisplayModeNormal()) {
    int edge_width = (browser_->window() && browser_->window()->IsMaximized()) ?
        0 : kPopupBackgroundEdge->width();  // See Paint().
    location_bar_->SetBounds(edge_width, PopupTopSpacing(),
        width() - (edge_width * 2), location_bar_->GetPreferredSize().height());
    return;
  }

  int child_y = std::min(kControlVertOffset, height());
  // We assume all child elements are the same height.
  int child_height =
      std::min(go_->GetPreferredSize().height(), height() - child_y);

  // If the window is maximized, we extend the back button to the left so that
  // clicking on the left-most pixel will activate the back button.
  // TODO(abarth):  If the window becomes maximized but is not resized,
  //                then Layout() might not be called and the back button
  //                will be slightly the wrong size.  We should force a
  //                Layout() in this case.
  //                http://crbug.com/5540
  int back_width = back_->GetPreferredSize().width();
  if (browser_->window() && browser_->window()->IsMaximized())
    back_->SetBounds(0, child_y, back_width + kControlIndent, child_height);
  else
    back_->SetBounds(kControlIndent, child_y, back_width, child_height);

  forward_->SetBounds(back_->x() + back_->width(), child_y,
                      forward_->GetPreferredSize().width(), child_height);

  reload_->SetBounds(forward_->x() + forward_->width() + kControlHorizOffset,
                     child_y, reload_->GetPreferredSize().width(),
                     child_height);

  if (show_home_button_.GetValue()) {
    home_->SetVisible(true);
    home_->SetBounds(reload_->x() + reload_->width() + kControlHorizOffset,
                     child_y, home_->GetPreferredSize().width(), child_height);
  } else {
    home_->SetVisible(false);
    home_->SetBounds(reload_->x() + reload_->width(), child_y, 0, child_height);
  }

  star_->SetBounds(home_->x() + home_->width() + kControlHorizOffset,
                   child_y, star_->GetPreferredSize().width(), child_height);

  int go_button_width = go_->GetPreferredSize().width();
  int page_menu_width = page_menu_->GetPreferredSize().width();
  int app_menu_width = app_menu_->GetPreferredSize().width();
  int bookmark_menu_width = bookmark_menu_ ?
      bookmark_menu_->GetPreferredSize().width() : 0;
  int location_x = star_->x() + star_->width();
  int available_width = width() - kPaddingRight - bookmark_menu_width -
      app_menu_width - page_menu_width - kMenuButtonOffset - go_button_width -
      location_x;
  location_bar_->SetBounds(location_x, child_y, std::max(available_width, 0),
                           child_height);

  go_->SetBounds(location_bar_->x() + location_bar_->width(), child_y,
                 go_button_width, child_height);

  int next_menu_x = go_->x() + go_->width() + kMenuButtonOffset;

  if (bookmark_menu_) {
    bookmark_menu_->SetBounds(next_menu_x, child_y, bookmark_menu_width,
                              child_height);
    next_menu_x += bookmark_menu_width;
  }

  page_menu_->SetBounds(next_menu_x, child_y, page_menu_width, child_height);
  next_menu_x += page_menu_width;

  app_menu_->SetBounds(next_menu_x, child_y, app_menu_width, child_height);
}

void BrowserToolbarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

  if (IsDisplayModeNormal())
    return;

  // In maximized mode, we don't draw the endcaps on the location bar, because
  // when they're flush against the edge of the screen they just look glitchy.
  if (!browser_->window() || !browser_->window()->IsMaximized()) {
    int top_spacing = PopupTopSpacing();
    canvas->DrawBitmapInt(*kPopupBackgroundEdge, 0, top_spacing);
    canvas->DrawBitmapInt(*kPopupBackgroundEdge,
                          width() - kPopupBackgroundEdge->width(), top_spacing);
  }

  // For glass, we need to draw a black line below the location bar to separate
  // it from the content area.  For non-glass, the NonClientView draws the
  // toolbar background below the location bar for us.
  if (GetWindow()->GetNonClientView()->UseNativeFrame())
    canvas->FillRectInt(SK_ColorBLACK, 0, height() - 1, width(), 1);
}

void BrowserToolbarView::DidGainFocus() {
#if defined(OS_WIN)
  // Check to see if MSAA focus should be restored to previously focused button,
  // and if button is an enabled, visibled child of toolbar.
  if (!acc_focused_view_ ||
      (acc_focused_view_->GetParent()->GetID() != VIEW_ID_TOOLBAR) ||
      !acc_focused_view_->IsEnabled() ||
      !acc_focused_view_->IsVisible()) {
    // Find first accessible child (-1 to start search at parent).
    int first_acc_child = GetNextAccessibleViewIndex(-1, false);

    // No buttons enabled or visible.
    if (first_acc_child == -1)
      return;

    set_acc_focused_view(GetChildViewAt(first_acc_child));
  }

  // Default focus is on the toolbar.
  int view_index = VIEW_ID_TOOLBAR;

  // Set hot-tracking for child, and update focused_view for MSAA focus event.
  if (acc_focused_view_) {
    acc_focused_view_->SetHotTracked(true);

    // Show the tooltip for the view that got the focus.
    if (GetWidget()->GetTooltipManager())
      GetWidget()->GetTooltipManager()->ShowKeyboardTooltip(acc_focused_view_);

    // Update focused_view with MSAA-adjusted child id.
    view_index = acc_focused_view_->GetID();
  }

  gfx::NativeView wnd = GetWidget()->GetNativeView();

  // Notify Access Technology that there was a change in keyboard focus.
  ::NotifyWinEvent(EVENT_OBJECT_FOCUS, wnd, OBJID_CLIENT,
                   static_cast<LONG>(view_index));
#else
  // TODO(port): deal with toolbar a11y focus.
  NOTIMPLEMENTED();
#endif
}

void BrowserToolbarView::WillLoseFocus() {
#if defined(OS_WIN)
  if (acc_focused_view_) {
    // Resetting focus state.
    acc_focused_view_->SetHotTracked(false);
  }
  // Any tooltips that are active should be hidden when toolbar loses focus.
  if (GetWidget() && GetWidget()->GetTooltipManager())
    GetWidget()->GetTooltipManager()->HideKeyboardTooltip();
#else
  // TODO(port): deal with toolbar a11y focus.
  NOTIMPLEMENTED();
#endif
}

bool BrowserToolbarView::OnKeyPressed(const views::KeyEvent& e) {
#if defined(OS_WIN)
  // Paranoia check, button should be initialized upon toolbar gaining focus.
  if (!acc_focused_view_)
    return false;

  int focused_view = GetChildIndex(acc_focused_view_);
  int next_view = focused_view;

  switch (e.GetCharacter()) {
    case VK_LEFT:
      next_view = GetNextAccessibleViewIndex(focused_view, true);
      break;
    case VK_RIGHT:
      next_view = GetNextAccessibleViewIndex(focused_view, false);
      break;
    case VK_DOWN:
    case VK_RETURN:
      // VK_SPACE is already handled by the default case.
      if (acc_focused_view_->GetID() == VIEW_ID_PAGE_MENU ||
          acc_focused_view_->GetID() == VIEW_ID_APP_MENU) {
        // If a menu button in toolbar is activated and its menu is displayed,
        // then active tooltip should be hidden.
        if (GetWidget()->GetTooltipManager())
          GetWidget()->GetTooltipManager()->HideKeyboardTooltip();
        // Safe to cast, given to above view id check.
        static_cast<views::MenuButton*>(acc_focused_view_)->Activate();
        if (!acc_focused_view_) {
          // Activate triggered a focus change, don't try to change focus.
          return true;
        }
        // Re-enable hot-tracking, as Activate() will disable it.
        acc_focused_view_->SetHotTracked(true);
        break;
      }
    default:
      // If key is not handled explicitly, pass it on to view.
      return acc_focused_view_->OnKeyPressed(e);
  }

  // No buttons enabled or visible.
  if (next_view == -1)
    return false;

  // Only send an event if focus moved.
  if (next_view != focused_view) {
    // Remove hot-tracking from old focused button.
    acc_focused_view_->SetHotTracked(false);

    // All is well, update the focused child member variable.
    acc_focused_view_ = GetChildViewAt(next_view);

    // Hot-track new focused button.
    acc_focused_view_->SetHotTracked(true);

    // Retrieve information to generate an MSAA focus event.
    int view_id = acc_focused_view_->GetID();
    gfx::NativeView wnd = GetWidget()->GetNativeView();

    // Show the tooltip for the view that got the focus.
    if (GetWidget()->GetTooltipManager()) {
      GetWidget()->GetTooltipManager()->
          ShowKeyboardTooltip(GetChildViewAt(next_view));
    }
    // Notify Access Technology that there was a change in keyboard focus.
    ::NotifyWinEvent(EVENT_OBJECT_FOCUS, wnd, OBJID_CLIENT,
                     static_cast<LONG>(view_id));
    return true;
  }
#else
  // TODO(port): deal with toolbar a11y focus.
  NOTIMPLEMENTED();
#endif
  return false;
}

bool BrowserToolbarView::OnKeyReleased(const views::KeyEvent& e) {
  // Paranoia check, button should be initialized upon toolbar gaining focus.
  if (!acc_focused_view_)
    return false;

  // Have keys be handled by the views themselves.
  return acc_focused_view_->OnKeyReleased(e);
}

gfx::Size BrowserToolbarView::GetPreferredSize() {
  if (IsDisplayModeNormal()) {
    int min_width = kControlIndent + back_->GetPreferredSize().width() +
        forward_->GetPreferredSize().width() + kControlHorizOffset +
        reload_->GetPreferredSize().width() + (show_home_button_.GetValue() ?
        (home_->GetPreferredSize().width() + kControlHorizOffset) : 0) +
        star_->GetPreferredSize().width() + go_->GetPreferredSize().width() +
        kMenuButtonOffset +
        (bookmark_menu_ ? bookmark_menu_->GetPreferredSize().width() : 0) +
        page_menu_->GetPreferredSize().width() +
        app_menu_->GetPreferredSize().width() + kPaddingRight;

    static SkBitmap normal_background;
    if (normal_background.isNull()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      normal_background = *rb.GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
    }

    return gfx::Size(min_width, normal_background.height());
  }

  int vertical_spacing = PopupTopSpacing() +
      (GetWindow()->GetNonClientView()->UseNativeFrame() ?
          kPopupBottomSpacingGlass : kPopupBottomSpacingNonGlass);
  return gfx::Size(0, location_bar_->GetPreferredSize().height() +
      vertical_spacing);
}

void BrowserToolbarView::RunPageMenu(const gfx::Point& pt,
                                     gfx::NativeView parent) {
  views::Menu::AnchorPoint anchor = views::Menu::TOPRIGHT;
  if (UILayoutIsRightToLeft())
    anchor = views::Menu::TOPLEFT;

  scoped_ptr<views::Menu> menu(views::Menu::Create(this, anchor, parent));
  menu->AppendMenuItemWithLabel(IDC_CREATE_SHORTCUTS,
      l10n_util::GetString(IDS_CREATE_SHORTCUTS));
  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_CUT, l10n_util::GetString(IDS_CUT));
  menu->AppendMenuItemWithLabel(IDC_COPY, l10n_util::GetString(IDS_COPY));
  menu->AppendMenuItemWithLabel(IDC_PASTE, l10n_util::GetString(IDS_PASTE));
  menu->AppendSeparator();

  menu->AppendMenuItemWithLabel(IDC_FIND,
                                l10n_util::GetString(IDS_FIND));
  menu->AppendMenuItemWithLabel(IDC_SAVE_PAGE,
                                l10n_util::GetString(IDS_SAVE_PAGE));
  menu->AppendMenuItemWithLabel(IDC_PRINT, l10n_util::GetString(IDS_PRINT));
  menu->AppendSeparator();

  views::Menu* zoom_menu = menu->AppendSubMenu(
      IDC_ZOOM_MENU, l10n_util::GetString(IDS_ZOOM_MENU));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_PLUS,
                                     l10n_util::GetString(IDS_ZOOM_PLUS));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_NORMAL,
                                     l10n_util::GetString(IDS_ZOOM_NORMAL));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_MINUS,
                                     l10n_util::GetString(IDS_ZOOM_MINUS));

  // Create encoding menu.
  views::Menu* encoding_menu = menu->AppendSubMenu(
      IDC_ENCODING_MENU, l10n_util::GetString(IDS_ENCODING_MENU));

  EncodingMenuControllerDelegate::BuildEncodingMenu(profile_, encoding_menu);

#if defined(OS_WIN)
  const struct MenuCreateMaterial {
    unsigned int menu_id;
    unsigned int menu_label_id;
  } developer_menu_materials[] = {
    { IDC_VIEW_SOURCE, IDS_VIEW_SOURCE },
    { IDC_DEBUGGER, IDS_DEBUGGER },
    { IDC_JS_CONSOLE, IDS_JS_CONSOLE },
    { IDC_TASK_MANAGER, IDS_TASK_MANAGER }
  };
  // Append developer menu.
  menu->AppendSeparator();
  views::Menu* developer_menu = menu->AppendSubMenu(IDC_DEVELOPER_MENU,
      l10n_util::GetString(IDS_DEVELOPER_MENU));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool new_tools = !command_line.HasSwitch(
      switches::kDisableOutOfProcessDevTools);

  for (int i = 0; i < arraysize(developer_menu_materials); ++i) {
    if (new_tools && developer_menu_materials[i].menu_id == IDC_DEBUGGER)
      continue;
    if (developer_menu_materials[i].menu_id) {
      developer_menu->AppendMenuItemWithLabel(
          developer_menu_materials[i].menu_id,
          l10n_util::GetString(developer_menu_materials[i].menu_label_id));
    } else {
      developer_menu->AppendSeparator();
    }
  }
#endif

  menu->AppendSeparator();

  menu->AppendMenuItemWithLabel(IDC_REPORT_BUG,
                               l10n_util::GetString(IDS_REPORT_BUG));
  menu->RunMenuAt(pt.x(), pt.y());
}

void BrowserToolbarView::RunAppMenu(const gfx::Point& pt,
                                    gfx::NativeView parent) {
  views::Menu::AnchorPoint anchor = views::Menu::TOPRIGHT;
  if (UILayoutIsRightToLeft())
    anchor = views::Menu::TOPLEFT;

  scoped_ptr<views::Menu> menu(views::Menu::Create(this, anchor, parent));
  menu->AppendMenuItemWithLabel(IDC_NEW_TAB, l10n_util::GetString(IDS_NEW_TAB));
  menu->AppendMenuItemWithLabel(IDC_NEW_WINDOW,
                                l10n_util::GetString(IDS_NEW_WINDOW));
  menu->AppendMenuItemWithLabel(IDC_NEW_INCOGNITO_WINDOW,
                                l10n_util::GetString(IDS_NEW_INCOGNITO_WINDOW));

  // Enumerate profiles asynchronously and then create the parent menu item.
  // We will create the child menu items for this once the asynchronous call is
  // done.  See OnGetProfilesDone().
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableUserDataDirProfiles)) {
    profiles_helper_->GetProfiles(NULL);
    profiles_menu_ = menu->AppendSubMenu(
        IDC_PROFILE_MENU, l10n_util::GetString(IDS_PROFILE_MENU));
  }

  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_SHOW_BOOKMARK_BAR,
                                l10n_util::GetString(IDS_SHOW_BOOKMARK_BAR));
  menu->AppendMenuItemWithLabel(IDC_FULLSCREEN,
                                l10n_util::GetString(IDS_FULLSCREEN));
  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_SHOW_HISTORY,
                                l10n_util::GetString(IDS_SHOW_HISTORY));
  menu->AppendMenuItemWithLabel(IDC_SHOW_BOOKMARK_MANAGER,
                                l10n_util::GetString(IDS_BOOKMARK_MANAGER));
  menu->AppendMenuItemWithLabel(IDC_SHOW_DOWNLOADS,
                                l10n_util::GetString(IDS_SHOW_DOWNLOADS));
  menu->AppendSeparator();
#ifdef CHROME_PERSONALIZATION
  if (!Personalization::IsP13NDisabled(profile_)) {
    menu->AppendMenuItemWithLabel(IDC_P13N_INFO,
        Personalization::GetMenuItemInfoText(browser()));
  }
#endif
  menu->AppendMenuItemWithLabel(IDC_CLEAR_BROWSING_DATA,
                                l10n_util::GetString(IDS_CLEAR_BROWSING_DATA));
  menu->AppendMenuItemWithLabel(IDC_IMPORT_SETTINGS,
                                l10n_util::GetString(IDS_IMPORT_SETTINGS));
  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_OPTIONS, l10n_util::GetStringF(IDS_OPTIONS,
                                l10n_util::GetString(IDS_PRODUCT_NAME)));
  menu->AppendMenuItemWithLabel(IDC_ABOUT, l10n_util::GetStringF(IDS_ABOUT,
                                l10n_util::GetString(IDS_PRODUCT_NAME)));
  menu->AppendMenuItemWithLabel(IDC_HELP_PAGE,
                                l10n_util::GetString(IDS_HELP_PAGE));
  menu->AppendSeparator();
  menu->AppendMenuItemWithLabel(IDC_EXIT, l10n_util::GetString(IDS_EXIT));

  menu->RunMenuAt(pt.x(), pt.y());

  // Menu is going away, so set the profiles menu pointer to NULL.
  profiles_menu_ = NULL;
}

bool BrowserToolbarView::IsItemChecked(int id) const {
  if (!profile_)
    return false;
  if (id == IDC_SHOW_BOOKMARK_BAR)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  return EncodingMenuControllerDelegate::IsItemChecked(id);
}

void BrowserToolbarView::RunMenu(views::View* source, const gfx::Point& pt,
                                 gfx::NativeView parent) {
  switch (source->GetID()) {
    case VIEW_ID_PAGE_MENU:
      RunPageMenu(pt, parent);
      break;
    case VIEW_ID_APP_MENU:
      RunAppMenu(pt, parent);
      break;
    default:
      NOTREACHED() << "Invalid source menu.";
  }
}

void BrowserToolbarView::OnGetProfilesDone(
    const std::vector<std::wstring>& profiles) {
  // Nothing to do if the menu has gone away.
  if (!profiles_menu_)
    return;

  // Store the latest list of profiles in the browser.
  browser_->set_user_data_dir_profiles(profiles);

  // Add direct sub menu items for profiles.
  std::vector<std::wstring>::const_iterator iter = profiles.begin();
  for (int i = IDC_NEW_WINDOW_PROFILE_0;
       (i <= IDC_NEW_WINDOW_PROFILE_LAST) && (iter != profiles.end());
       ++i, ++iter)
    profiles_menu_->AppendMenuItemWithLabel(i, *iter);

  // If there are more profiles then show "Other" link.
  if (iter != profiles.end()) {
    profiles_menu_->AppendSeparator();
    profiles_menu_->AppendMenuItemWithLabel(
        IDC_SELECT_PROFILE, l10n_util::GetString(IDS_SELECT_PROFILE));
  }

  // Always show a link to select a new profile.
  profiles_menu_->AppendSeparator();
  profiles_menu_->AppendMenuItemWithLabel(
      IDC_NEW_PROFILE,
      l10n_util::GetString(IDS_SELECT_PROFILE_DIALOG_NEW_PROFILE_ENTRY));
}

bool BrowserToolbarView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    (*name).assign(accessible_name_);
    return true;
  }
  return false;
}

bool BrowserToolbarView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TOOLBAR;
  return true;
}

void BrowserToolbarView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

void BrowserToolbarView::ThemeChanged() {
  LoadLeftSideControlsImages();
  LoadCenterStackImages();
  LoadRightSideControlsImages();
}

int BrowserToolbarView::GetNextAccessibleViewIndex(int view_index,
                                                   bool nav_left) {
  int modifier = 1;

  if (nav_left)
    modifier = -1;

  int current_view_index = view_index + modifier;

  while ((current_view_index >= 0) &&
         (current_view_index < GetChildViewCount())) {
    // Skip the location bar, as it has its own keyboard navigation. Also skip
    // any views that cannot be interacted with.
    if (current_view_index == GetChildIndex(location_bar_) ||
        !GetChildViewAt(current_view_index)->IsEnabled() ||
        !GetChildViewAt(current_view_index)->IsVisible()) {
      current_view_index += modifier;
      continue;
    }
    // Update view_index with the available button index found.
    view_index = current_view_index;
    break;
  }
  // Returns the next available button index, or if no button is available in
  // the specified direction, remains where it was.
  return view_index;
}

void BrowserToolbarView::ShowContextMenu(int x, int y, bool is_mouse_gesture) {
  if (acc_focused_view_)
    acc_focused_view_->ShowContextMenu(x, y, is_mouse_gesture);
}

int BrowserToolbarView::GetDragOperations(views::View* sender, int x, int y) {
  DCHECK(sender == star_);
  if (!tab_ || !tab_->ShouldDisplayURL() || !tab_->GetURL().is_valid()) {
    return DragDropTypes::DRAG_NONE;
  }
  if (profile_ && profile_->GetBookmarkModel() &&
      profile_->GetBookmarkModel()->IsBookmarked(tab_->GetURL())) {
    return DragDropTypes::DRAG_MOVE | DragDropTypes::DRAG_COPY |
           DragDropTypes::DRAG_LINK;
  }
  return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
}

void BrowserToolbarView::WriteDragData(views::View* sender,
                                       int press_x,
                                       int press_y,
                                       OSExchangeData* data) {
  DCHECK(
      GetDragOperations(sender, press_x, press_y) != DragDropTypes::DRAG_NONE);

  UserMetrics::RecordAction(L"Toolbar_DragStar", profile_);

#if defined(OS_WIN)
  // If there is a bookmark for the URL, add the bookmark drag data for it. We
  // do this to ensure the bookmark is moved, rather than creating an new
  // bookmark.
  if (profile_ && profile_->GetBookmarkModel()) {
    BookmarkNode* node = profile_->GetBookmarkModel()->
        GetMostRecentlyAddedNodeForURL(tab_->GetURL());
    if (node) {
      BookmarkDragData bookmark_data(node);
      bookmark_data.Write(profile_, data);
    }
  }

  drag_utils::SetURLAndDragImage(tab_->GetURL(),
                                 UTF16ToWideHack(tab_->GetTitle()),
                                 tab_->GetFavIcon(),
                                 data);
#else
  // TODO(port): do bookmark item drag & drop
  NOTIMPLEMENTED();
#endif
}

TabContents* BrowserToolbarView::GetTabContents() {
  return tab_;
}

void BrowserToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  views::Button* button = NULL;
  switch (id) {
    case IDC_BACK:
      button = back_;
      break;
    case IDC_FORWARD:
      button = forward_;
      break;
    case IDC_RELOAD:
      button = reload_;
      break;
    case IDC_HOME:
      button = home_;
      break;
    case IDC_STAR:
      button = star_;
      break;
  }
  if (button)
    button->SetEnabled(enabled);
}

void BrowserToolbarView::ButtonPressed(views::Button* sender) {
  browser_->ExecuteCommandWithDisposition(
      sender->tag(),
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

gfx::Rect BrowserToolbarView::GetPopupBounds() const {
  gfx::Point origin;
  views::View::ConvertPointToScreen(star_, &origin);
  origin.set_y(origin.y() + star_->height() + kOmniboxPopupVerticalSpacing);
  gfx::Rect popup_bounds(origin.x(), origin.y(),
                         star_->width() + location_bar_->width() + go_->width(),
                         0);
  if (UILayoutIsRightToLeft()) {
    popup_bounds.set_x(
        popup_bounds.x() - location_bar_->width() - go_->width());
  } else {
    popup_bounds.set_x(popup_bounds.x());
  }
  popup_bounds.set_y(popup_bounds.y());
  popup_bounds.set_width(popup_bounds.width());
  // Inset the bounds a little, since the buttons on either edge of the omnibox
  // have invisible padding that makes the popup appear too wide.
  popup_bounds.Inset(kOmniboxButtonsHorizontalMargin, 0);
  return popup_bounds;
}

// static
int BrowserToolbarView::PopupTopSpacing() {
  return GetWindow()->GetNonClientView()->UseNativeFrame() ?
      0 : kPopupTopSpacingNonGlass;
}

void BrowserToolbarView::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      Layout();
      SchedulePaint();
    }
  }
}

bool BrowserToolbarView::GetAcceleratorInfo(int id,
                                            views::Accelerator* accel) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (id) {
    case IDC_CUT:
      *accel = views::Accelerator(L'X', false, true, false);
      return true;
    case IDC_COPY:
      *accel = views::Accelerator(L'C', false, true, false);
      return true;
    case IDC_PASTE:
      *accel = views::Accelerator(L'V', false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetWidget()->GetAccelerator(id, accel);
}
