// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Implementation of BlockedPopupContainer and its corresponding View
// class. The BlockedPopupContainer is the sort of Model class which owns the
// blocked popups' TabContents (but like most Chromium interface code, it there
// isn't a strict Model/View separation), and BlockedPopupContainerView
// presents the user interface controls, creates and manages the popup menu.

#include "chrome/browser/views/blocked_popup_container.h"

#include <math.h>

#include "base/string_util.h"
#include "grit/theme_resources.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/button.h"
#include "chrome/views/menu_button.h"

#include "generated_resources.h"

namespace {
// Menu item ID for the "Notify me when a popup is blocked" checkbox. (All
// other menu IDs are positive and should be base 1 indexes into the vector of
// blocked popups.)
const int kNotifyMenuItem = -1;

// A number larger than the internal popup count on the Renderer; meant for
// preventing a compromised renderer from exhausting GDI memory by spawning
// infinite windows.
const int kImpossibleNumberOfPopups = 30;

// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

// The background color of the blocked popup notification.
const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

// The border color of the blocked popup notification. This is the same as the
// border around the inside of the tab contents.
const SkColor kBorderColor = SkColorSetRGB(190, 205, 223);
// Thickness of the border.
const int kBorderSize = 1;

// Duration of the showing/hiding animations.
const int kShowAnimationDurationMS = 200;
const int kHideAnimationDurationMS = 120;
const int kFramerate = 25;

// views::MenuButton requires that the largest string be passed in to its
// constructor to reserve space. "99" should preallocate enough space for all
// numbers.
const int kWidestNumber = 99;

// Rounded corner radius (in pixels).
const int kBackgroundCornerRadius = 4;

// Rounded corner definition so the top corners are rounded, and the bottom are
// normal 90 degree angles.
const SkScalar kRoundedCornerRad[8] = {
  // Top left corner
  SkIntToScalar(kBackgroundCornerRadius),
  SkIntToScalar(kBackgroundCornerRadius),
  // Top right corner
  SkIntToScalar(kBackgroundCornerRadius),
  SkIntToScalar(kBackgroundCornerRadius),
  // Bottom right corner
  0,
  0,
  // Bottom left corner
  0,
  0
};

}

BlockedPopupContainerView::BlockedPopupContainerView(
    BlockedPopupContainer* container)
    : container_(container) {
  ResourceBundle &resource_bundle = ResourceBundle::GetSharedInstance();

  // Create a button with a multidigit number to reserve space.
  popup_count_label_ = new views::MenuButton(
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                            IntToWString(kWidestNumber)),
      NULL, true);
  popup_count_label_->SetTextAlignment(views::TextButton::ALIGN_CENTER);
  popup_count_label_->SetListener(this, 1);
  AddChildView(popup_count_label_);

  // For now, we steal the Find close button, since it looks OK.
  close_button_ = new views::Button();
  close_button_->SetFocusable(true);
  close_button_->SetImage(views::Button::BS_NORMAL,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::Button::BS_HOT,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::Button::BS_PUSHED,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetListener(this, 0);
  AddChildView(close_button_);

  set_background(views::Background::CreateStandardPanelBackground());
  UpdatePopupCountLabel();
}

BlockedPopupContainerView::~BlockedPopupContainerView() {
}

void BlockedPopupContainerView::UpdatePopupCountLabel() {
  popup_count_label_->SetText(container_->GetWindowTitle());
  Layout();
  SchedulePaint();
}

void BlockedPopupContainerView::Paint(ChromeCanvas* canvas) {
  // Draw the standard background.
  View::Paint(canvas);

  SkRect rect;
  rect.set(0, 0, SkIntToScalar(width()), SkIntToScalar(height()));

  // Draw the border
  SkPaint border_paint;
  border_paint.setFlags(SkPaint::kAntiAlias_Flag);
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(kBorderColor);
  SkPath border_path;
  border_path.addRoundRect(rect, kRoundedCornerRad, SkPath::kCW_Direction);
  canvas->drawPath(border_path, border_paint);
}

void BlockedPopupContainerView::Layout() {
  gfx::Size panel_size = GetPreferredSize();
  gfx::Size button_size = close_button_->GetPreferredSize();
  gfx::Size size = popup_count_label_->GetPreferredSize();

  popup_count_label_->SetBounds(kSmallPadding, kSmallPadding,
                                size.width(),
                                size.height());

  int close_button_padding =
      static_cast<int>(ceil(panel_size.height() / 2.0) -
                       ceil(button_size.height() / 2.0));
  close_button_->SetBounds(width() - button_size.width() - close_button_padding,
                           close_button_padding,
                           button_size.width(),
                           button_size.height());
}

gfx::Size BlockedPopupContainerView::GetPreferredSize() {
  gfx::Size preferred_size = popup_count_label_->GetPreferredSize();
  preferred_size.Enlarge(close_button_->GetPreferredSize().width(), 0);
  // Add padding to all sides of the |popup_count_label_| except the right.
  preferred_size.Enlarge(kSmallPadding, 2 * kSmallPadding);

  // Add padding to the left and right side of |close_button_| equal to its
  // horizontal/vertical spacing.
  gfx::Size button_size = close_button_->GetPreferredSize();
  int close_button_padding =
      static_cast<int>(ceil(preferred_size.height() / 2.0) -
                       ceil(button_size.height() / 2.0));
  preferred_size.Enlarge(2 * close_button_padding, 0);

  return preferred_size;
}

void BlockedPopupContainerView::ButtonPressed(views::BaseButton* sender) {
  if (sender == popup_count_label_) {
    launch_menu_.reset(new Menu(this, Menu::TOPLEFT, container_->GetHWND()));

    int item_count = container_->GetTabContentsCount();
    for (int i = 0; i < item_count; ++i) {
      std::wstring label = container_->GetDisplayStringForItem(i);
      // We can't just use the index into container_ here because Menu reserves
      // the value 0 as the nop command.
      launch_menu_->AppendMenuItem(i + 1, label, Menu::NORMAL);
    }

    launch_menu_->AppendSeparator();
    launch_menu_->AppendMenuItem(
        kNotifyMenuItem,
        l10n_util::GetString(IDS_OPTIONS_SHOWPOPUPBLOCKEDNOTIFICATION),
        Menu::NORMAL);

    CPoint cursor_position;
    ::GetCursorPos(&cursor_position);
    launch_menu_->RunMenuAt(cursor_position.x, cursor_position.y);
  } else if (sender == close_button_) {
    container_->set_dismissed();
    container_->CloseAllPopups();
  }
}

bool BlockedPopupContainerView::IsItemChecked(int id) const {
  if (id == kNotifyMenuItem)
    return container_->GetShowBlockedPopupNotification();

  return false;
}

void BlockedPopupContainerView::ExecuteCommand(int id) {
  if (id == kNotifyMenuItem) {
    container_->ToggleBlockedPopupNotification();
  } else {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    container_->LaunchPopupIndex(id - 1);
  }
}

// static
BlockedPopupContainer* BlockedPopupContainer::Create(
    TabContents* owner, Profile* profile, const gfx::Point& initial_anchor) {
  BlockedPopupContainer* container = new BlockedPopupContainer(owner, profile);
  container->Init(initial_anchor);
  return container;
}

BlockedPopupContainer::~BlockedPopupContainer() {
}

BlockedPopupContainer::BlockedPopupContainer(TabContents* owner,
                                             Profile* profile)
    : Animation(kFramerate, NULL),
      owner_(owner),
      container_view_(NULL),
      has_been_dismissed_(false),
      in_show_animation_(false),
      visibility_percentage_(0) {
  block_popup_pref_.Init(
      prefs::kBlockPopups, profile->GetPrefs(), NULL);
}

void BlockedPopupContainer::ToggleBlockedPopupNotification() {
  bool current = block_popup_pref_.GetValue();
  block_popup_pref_.SetValue(!current);
}

bool BlockedPopupContainer::GetShowBlockedPopupNotification() {
  return !block_popup_pref_.GetValue();
}

void BlockedPopupContainer::AddTabContents(TabContents* blocked_contents,
                                           const gfx::Rect& bounds) {
  if (has_been_dismissed_) {
    // We simply bounce this popup without notice.
    blocked_contents->CloseContents();
    return;
  }

  if (blocked_popups_.size() > kImpossibleNumberOfPopups) {
    blocked_contents->CloseContents();
    LOG(INFO) << "Warning: Renderer is sending more popups to us then should be"
        " possible. Renderer compromised?";
    return;
  }

  blocked_contents->set_delegate(this);
  blocked_popups_.push_back(std::make_pair(blocked_contents, bounds));
  container_view_->UpdatePopupCountLabel();

  ShowSelf();
}

void BlockedPopupContainer::LaunchPopupIndex(int index) {
  if (static_cast<size_t>(index) < blocked_popups_.size()) {
    TabContents* contents = blocked_popups_[index].first;
    gfx::Rect bounds = blocked_popups_[index].second;
    blocked_popups_.erase(blocked_popups_.begin() + index);
    container_view_->UpdatePopupCountLabel();

    contents->set_delegate(NULL);
    contents->DisassociateFromPopupCount();

    // Pass this TabContents back to our owner, forcing the window to be
    // displayed since user_gesture is true.
    owner_->AddNewContents(contents, NEW_POPUP, bounds, true);
  }

  if (blocked_popups_.size() == 0)
    CloseAllPopups();
}

int BlockedPopupContainer::GetTabContentsCount() const {
  return blocked_popups_.size();
}

std::wstring BlockedPopupContainer::GetDisplayStringForItem(int index) {
  const GURL& url = blocked_popups_[index].first->GetURL().GetOrigin();

  std::wstring label =
      l10n_util::GetStringF(IDS_POPUP_TITLE_FORMAT,
                            UTF8ToWide(url.possibly_invalid_spec()),
                            blocked_popups_[index].first->GetTitle());
  return label;
}

void BlockedPopupContainer::CloseAllPopups() {
  CloseEachTabContents();
  owner_->PopupNotificationVisibilityChanged(false);
  container_view_->UpdatePopupCountLabel();
  HideSelf();
}

// Overridden from ConstrainedWindow:
void BlockedPopupContainer::CloseConstrainedWindow() {
  CloseEachTabContents();

  // Broadcast to all observers of NOTIFY_CWINDOW_CLOSED.
  // One example of such an observer is AutomationCWindowTracker in the
  // automation component.
  NotificationService::current()->Notify(NotificationType::CWINDOW_CLOSED,
                                         Source<ConstrainedWindow>(this),
                                         NotificationService::NoDetails());

  Close();
}

void BlockedPopupContainer::RepositionConstrainedWindowTo(
    const gfx::Point& anchor_point) {
  anchor_point_ = anchor_point;
  SetPosition();
}

std::wstring BlockedPopupContainer::GetWindowTitle() const {
  return l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                               IntToWString(GetTabContentsCount()));
}

const gfx::Rect& BlockedPopupContainer::GetCurrentBounds() const {
  return bounds_;
}

// Overridden from TabContentsDelegate:
void BlockedPopupContainer::OpenURLFromTab(TabContents* source,
                                           const GURL& url,
                                           const GURL& referrer,
                                           WindowOpenDisposition disposition,
                                           PageTransition::Type transition) {
  owner_->OpenURL(url, referrer, disposition, transition);
}

void BlockedPopupContainer::ReplaceContents(TabContents* source,
                                            TabContents* new_contents) {
  // Walk the vector to find the correct TabContents and replace it.
  bool found = false;
  gfx::Rect rect;
  for (std::vector<std::pair<TabContents*, gfx::Rect> >::iterator it =
           blocked_popups_.begin(); it != blocked_popups_.end(); ++it) {
    if (it->first == source) {
      it->first->set_delegate(NULL);
      rect = it->second;
      found = true;
      blocked_popups_.erase(it);
      break;
    }
  }

  if (found) {
    new_contents->set_delegate(this);
    blocked_popups_.push_back(std::make_pair(new_contents, rect));
  }
}

void BlockedPopupContainer::AddNewContents(TabContents* source,
                                           TabContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_position,
                                           bool user_gesture) {
  owner_->AddNewContents(new_contents, disposition, initial_position,
                         user_gesture);
}

void BlockedPopupContainer::CloseContents(TabContents* source) {
  for (std::vector<std::pair<TabContents*, gfx::Rect> >::iterator it =
           blocked_popups_.begin(); it != blocked_popups_.end(); ++it) {
    if (it->first == source) {
      it->first->set_delegate(NULL);
      blocked_popups_.erase(it);
      break;
    }
  }

  if (blocked_popups_.size() == 0)
    CloseAllPopups();
}

void BlockedPopupContainer::MoveContents(TabContents* source,
                                         const gfx::Rect& position) {
  for (std::vector<std::pair<TabContents*, gfx::Rect> >::iterator it =
           blocked_popups_.begin(); it != blocked_popups_.end(); ++it) {
    if (it->first == source) {
      it->second = position;
      break;
    }
  }
}

bool BlockedPopupContainer::IsPopup(TabContents* source) {
  return true;
}

TabContents* BlockedPopupContainer::GetConstrainingContents(
    TabContents* source) {
  return owner_;
}

// Overridden from Animation:
void BlockedPopupContainer::AnimateToState(double state) {
  if (in_show_animation_)
    visibility_percentage_ = state;
  else
    visibility_percentage_ = 1 - state;

  SetPosition();
}

// Override from views::WidgetWin:
void BlockedPopupContainer::OnFinalMessage(HWND window) {
  owner_->WillClose(this);
  CloseEachTabContents();
  WidgetWin::OnFinalMessage(window);
}

void BlockedPopupContainer::OnSize(UINT param, const CSize& size) {
  // Set the window region so we have rounded corners on the top.
  SkRect rect;
  rect.set(0, 0, SkIntToScalar(size.cx), SkIntToScalar(size.cy));
  gfx::Path path;
  path.addRoundRect(rect, kRoundedCornerRad, SkPath::kCW_Direction);
  SetWindowRgn(path.CreateHRGN(), TRUE);

  ChangeSize(param, size);
}

// private:

void BlockedPopupContainer::Init(const gfx::Point& initial_anchor) {
  container_view_ = new BlockedPopupContainerView(this);
  container_view_->SetVisible(true);

  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  WidgetWin::Init(owner_->GetNativeView(), gfx::Rect(), false);
  SetContentsView(container_view_);
  RepositionConstrainedWindowTo(initial_anchor);

  if (GetShowBlockedPopupNotification())
    ShowSelf();
  else
    has_been_dismissed_ = true;
}

void BlockedPopupContainer::HideSelf() {
  in_show_animation_ = false;
  Animation::SetDuration(kHideAnimationDurationMS);
  Animation::Start();
}

void BlockedPopupContainer::ShowSelf() {
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  if (!Animation::IsAnimating() && visibility_percentage_ < 1.0) {
    in_show_animation_ = true;
    Animation::SetDuration(kShowAnimationDurationMS);
    Animation::Start();
  }
}

void BlockedPopupContainer::SetPosition() {
  gfx::Size size = container_view_->GetPreferredSize();
  int base_x = anchor_point_.x() - size.width();
  int base_y = anchor_point_.y() - size.height();
  // The bounds we report through the automation system are the real bounds;
  // the animation is short lived...
  bounds_ = gfx::Rect(gfx::Point(base_x, base_y), size);

  int real_height = static_cast<int>(size.height() * visibility_percentage_);
  int real_y = anchor_point_.y() - real_height;

  // Size this window to the bottom left corner starting at the anchor point.
  if (real_height > 0) {
    SetWindowPos(HWND_TOP, base_x, real_y, size.width(), real_height, 0);
    container_view_->SchedulePaint();
  } else {
    SetWindowPos(HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
  }
}

void BlockedPopupContainer::CloseEachTabContents() {
  while (!blocked_popups_.empty()) {
    blocked_popups_.back().first->set_delegate(NULL);
    blocked_popups_.back().first->CloseContents();
    blocked_popups_.pop_back();
  }

  blocked_popups_.clear();
}
