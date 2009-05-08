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

#include "app/gfx/chrome_canvas.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"

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
      this,
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                            IntToWString(kWidestNumber)),
      NULL, true);
  popup_count_label_->set_alignment(views::TextButton::ALIGN_CENTER);
  AddChildView(popup_count_label_);

  // For now, we steal the Find close button, since it looks OK.
  close_button_ = new views::ImageButton(this);
  close_button_->SetFocusable(true);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      resource_bundle.GetBitmapNamed(IDR_CLOSE_BAR_P));
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

void BlockedPopupContainerView::ButtonPressed(views::Button* sender) {
  if (sender == popup_count_label_) {
    launch_menu_.reset(new Menu(this, Menu::TOPLEFT,
                                container_->GetNativeView()));

    // Set items 1 .. popup_count as individual popups.
    int popup_count = container_->GetBlockedPopupCount();
    for (int i = 0; i < popup_count; ++i) {
      std::wstring url, title;
      container_->GetURLAndTitleForPopup(i, &url, &title);
      // We can't just use the index into container_ here because Menu reserves
      // the value 0 as the nop command.
      launch_menu_->AppendMenuItem(i + 1,
          l10n_util::GetStringF(IDS_POPUP_TITLE_FORMAT, url, title),
          Menu::NORMAL);
    }

    // Set items (kImpossibleNumberOfPopups + 1) ..
    // (kImpossibleNumberOfPopups + 1 + hosts.size()) as hosts.
    std::vector<std::wstring> hosts(container_->GetHosts());
    if (!hosts.empty() && (popup_count > 0))
      launch_menu_->AppendSeparator();
    for (size_t i = 0; i < hosts.size(); ++i) {
      launch_menu_->AppendMenuItem(kImpossibleNumberOfPopups + i + 1,
          l10n_util::GetStringF(IDS_POPUP_HOST_FORMAT, hosts[i]), Menu::NORMAL);
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
    container_->CloseAll();
  }
}

bool BlockedPopupContainerView::IsItemChecked(int id) const {
  if (id == kNotifyMenuItem)
    return container_->GetShowBlockedPopupNotification();

  if (id > kImpossibleNumberOfPopups)
    return container_->IsHostWhitelisted(id - kImpossibleNumberOfPopups - 1);

  return false;
}

void BlockedPopupContainerView::ExecuteCommand(int id) {
  if (id == kNotifyMenuItem) {
    container_->ToggleBlockedPopupNotification();
  } else if (id > kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    container_->ToggleWhitelistingForHost(id - kImpossibleNumberOfPopups - 1);
  } else {
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
                                           const gfx::Rect& bounds,
                                           const std::string& host) {
  if (has_been_dismissed_) {
    // We simply bounce this popup without notice.
    delete blocked_contents;
    return;
  }

  if (blocked_popups_.size() >= kImpossibleNumberOfPopups) {
    delete blocked_contents;
    LOG(INFO) << "Warning: Renderer is sending more popups to us than should be"
        " possible. Renderer compromised?";
    return;
  }

  blocked_contents->set_delegate(this);
  blocked_popups_.push_back(BlockedPopup(blocked_contents, bounds, host));

  PopupHosts::iterator i(popup_hosts_.find(host));
  if (i == popup_hosts_.end())
    popup_hosts_[host] = false;
  else
    DCHECK(!i->second);  // This host was already reported as whitelisted!

  container_view_->UpdatePopupCountLabel();
  ShowSelf();
}

void BlockedPopupContainer::OnPopupOpenedFromWhitelistedHost(
    const std::string& host) {
  if (has_been_dismissed_)
    return;

  PopupHosts::const_iterator i(popup_hosts_.find(host));
  if (i == popup_hosts_.end()) {
    popup_hosts_[host] = true;
    ShowSelf();
  } else {
    DCHECK(i->second);  // This host was already reported as not whitelisted!
  }
}

void BlockedPopupContainer::LaunchPopupIndex(int index) {
  if (static_cast<size_t>(index) >= blocked_popups_.size())
    return;

  BlockedPopups::iterator i(blocked_popups_.begin() + index);
  TabContents* contents = i->tab_contents;
  gfx::Rect bounds(i->bounds);
  EraseHostIfNeeded(i);
  blocked_popups_.erase(i);

  contents->set_delegate(NULL);
  contents->DisassociateFromPopupCount();

  // Pass this TabContents back to our owner, forcing the window to be
  // displayed since user_gesture is true.
  owner_->AddNewContents(contents, NEW_POPUP, bounds, true, GURL());

  container_view_->UpdatePopupCountLabel();
  if (blocked_popups_.empty() && popup_hosts_.empty())
    HideSelf();
}

int BlockedPopupContainer::GetBlockedPopupCount() const {
  return blocked_popups_.size();
}

void BlockedPopupContainer::GetURLAndTitleForPopup(int index,
                                                   std::wstring* url,
                                                   std::wstring* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = blocked_popups_[index].tab_contents;
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToWide(tab_contents_url.possibly_invalid_spec());
  *title = UTF16ToWideHack(tab_contents->GetTitle());
}

std::vector<std::wstring> BlockedPopupContainer::GetHosts() const {
  std::vector<std::wstring> hosts;
  for (PopupHosts::const_iterator i(popup_hosts_.begin());
       i != popup_hosts_.end(); ++i)
    hosts.push_back(UTF8ToWide(i->first));
  return hosts;
}

bool BlockedPopupContainer::IsHostWhitelisted(int index) const {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  return (i == popup_hosts_.end()) ? false : i->second;
}

void BlockedPopupContainer::ToggleWhitelistingForHost(int index) {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  const std::string& host = i->first;
  bool should_whitelist = !i->second;
  owner_->SetWhitelistForHost(host, should_whitelist);
  if (should_whitelist) {
    for (int j = blocked_popups_.size() - 1; j >= 0; --j) {
      if (blocked_popups_[j].host == host)
        LaunchPopupIndex(j);
    }
  } else {
    // TODO(pkasting): Should we have some kind of handle to the open popups, so
    // we can hide them all here?
    popup_hosts_.erase(host);  // Can't use |i| because it's a const_iterator.
    if (blocked_popups_.empty() && popup_hosts_.empty())
      HideSelf();
  }
  // At this point i is invalid, since the item it points to was deleted (on
  // both conditional arms).
}

void BlockedPopupContainer::CloseAll() {
  ClearData();
  HideSelf();
}

// Overridden from ConstrainedWindow:
void BlockedPopupContainer::CloseConstrainedWindow() {
  ClearData();

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
                               IntToWString(GetBlockedPopupCount()));
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

void BlockedPopupContainer::AddNewContents(TabContents* source,
                                           TabContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_position,
                                           bool user_gesture) {
  owner_->AddNewContents(new_contents, disposition, initial_position,
                         user_gesture, GURL());
}

void BlockedPopupContainer::CloseContents(TabContents* source) {
  for (BlockedPopups::iterator it = blocked_popups_.begin();
       it != blocked_popups_.end(); ++it) {
    if (it->tab_contents == source) {
      it->tab_contents->set_delegate(NULL);
      EraseHostIfNeeded(it);
      blocked_popups_.erase(it);
      container_view_->UpdatePopupCountLabel();
      break;
    }
  }

  if (blocked_popups_.empty())
    HideSelf();
}

void BlockedPopupContainer::MoveContents(TabContents* source,
                                         const gfx::Rect& new_bounds) {
  for (BlockedPopups::iterator it = blocked_popups_.begin();
       it != blocked_popups_.end(); ++it) {
    if (it->tab_contents == source) {
      it->bounds = new_bounds;
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

ExtensionFunctionDispatcher* BlockedPopupContainer::
    CreateExtensionFunctionDispatcher(RenderViewHost* render_view_host,
                                      const std::string& extension_id) {
  return new ExtensionFunctionDispatcher(render_view_host, NULL, extension_id);
}

// Overridden from Animation:
void BlockedPopupContainer::AnimateToState(double state) {
  visibility_percentage_ = in_show_animation_ ? state : (1 - state);
  SetPosition();
}

// Override from views::WidgetWin:
void BlockedPopupContainer::OnFinalMessage(HWND window) {
  owner_->WillClose(this);
  ClearData();
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
  owner_->PopupNotificationVisibilityChanged(false);
}

void BlockedPopupContainer::ShowSelf() {
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  if (!Animation::IsAnimating() && visibility_percentage_ < 1.0) {
    in_show_animation_ = true;
    Animation::SetDuration(kShowAnimationDurationMS);
    Animation::Start();
  }
  owner_->PopupNotificationVisibilityChanged(true);
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

void BlockedPopupContainer::ClearData() {
  for (BlockedPopups::iterator i(blocked_popups_.begin());
       i != blocked_popups_.end(); ++i) {
    TabContents* tab_contents = i->tab_contents;
    tab_contents->set_delegate(NULL);
    delete tab_contents;
  }

  blocked_popups_.clear();
  popup_hosts_.clear();
}

BlockedPopupContainer::PopupHosts::const_iterator
    BlockedPopupContainer::ConvertHostIndexToIterator(int index) const {
  if (static_cast<size_t>(index) >= popup_hosts_.size())
    return popup_hosts_.end();
  // If only there was a std::map::const_iterator::operator +=() ...
  PopupHosts::const_iterator i(popup_hosts_.begin());
  for (int j = 0; j < index; ++j)
    ++i;
  return i;
}

void BlockedPopupContainer::EraseHostIfNeeded(BlockedPopups::iterator i) {
  const std::string& host = i->host;
  if (host.empty())
    return;
  for (BlockedPopups::const_iterator j(blocked_popups_.begin());
       j != blocked_popups_.end(); ++j) {
    if ((j != i) && (j->host == host))
      return;
  }
  popup_hosts_.erase(host);
}
