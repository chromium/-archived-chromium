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

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"

namespace {
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

// So that the MenuButton doesn't change its size as its text changes, during
// construction we feed it the strings it will be displaying, so it can set the
// max text width to the right value.  "99" should preallocate enough space for
// all numbers we'd show.
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
  // Now set the text to the other possible display string so that the button
  // will update its max text width (in case this string is longer).
  popup_count_label_->SetText(l10n_util::GetString(IDS_POPUPS_UNBLOCKED));
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
  UpdateLabel();
}

BlockedPopupContainerView::~BlockedPopupContainerView() {
}

void BlockedPopupContainerView::UpdateLabel() {
  size_t blocked_popups = container_->GetBlockedPopupCount();
  popup_count_label_->SetText((blocked_popups > 0) ?
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                            UintToWString(blocked_popups)) :
      l10n_util::GetString(IDS_POPUPS_UNBLOCKED));
  Layout();
  SchedulePaint();
}

void BlockedPopupContainerView::Paint(gfx::Canvas* canvas) {
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
    launch_menu_.reset(views::Menu::Create(this, views::Menu::TOPLEFT,
                                           container_->GetNativeView()));

    // Set items 1 .. popup_count as individual popups.
    size_t popup_count = container_->GetBlockedPopupCount();
    for (size_t i = 0; i < popup_count; ++i) {
      std::wstring url, title;
      container_->GetURLAndTitleForPopup(i, &url, &title);
      // We can't just use the index into container_ here because Menu reserves
      // the value 0 as the nop command.
      launch_menu_->AppendMenuItem(i + 1,
          l10n_util::GetStringF(IDS_POPUP_TITLE_FORMAT, url, title),
          views::Menu::NORMAL);
    }

    // Set items (kImpossibleNumberOfPopups + 1) ..
    // (kImpossibleNumberOfPopups + 1 + hosts.size()) as hosts.
    std::vector<std::wstring> hosts(container_->GetHosts());
    if (!hosts.empty() && (popup_count > 0))
      launch_menu_->AppendSeparator();
    for (size_t i = 0; i < hosts.size(); ++i) {
      launch_menu_->AppendMenuItem(kImpossibleNumberOfPopups + i + 1,
          l10n_util::GetStringF(IDS_POPUP_HOST_FORMAT, hosts[i]),
          views::Menu::NORMAL);
    }

    CPoint cursor_position;
    ::GetCursorPos(&cursor_position);
    launch_menu_->RunMenuAt(cursor_position.x, cursor_position.y);
  } else if (sender == close_button_) {
    container_->set_dismissed();
    container_->CloseAll();
  }
}

bool BlockedPopupContainerView::IsItemChecked(int id) const {
  if (id > kImpossibleNumberOfPopups) {
    return container_->IsHostWhitelisted(static_cast<size_t>(
        id - kImpossibleNumberOfPopups - 1));
  }

  return false;
}

void BlockedPopupContainerView::ExecuteCommand(int id) {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    container_->ToggleWhitelistingForHost(
        id_size_t - kImpossibleNumberOfPopups - 1);
  } else {
    container_->LaunchPopupAtIndex(id_size_t - 1);
  }
}

BlockedPopupContainer::~BlockedPopupContainer() {
}

// static
void BlockedPopupContainer::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
}

// static
BlockedPopupContainer* BlockedPopupContainer::Create(
    TabContents* owner, Profile* profile, const gfx::Point& initial_anchor) {
  BlockedPopupContainer* container =
      new BlockedPopupContainer(owner, profile->GetPrefs());
  container->Init(initial_anchor);
  return container;
}

void BlockedPopupContainer::AddTabContents(TabContents* tab_contents,
                                           const gfx::Rect& bounds,
                                           const std::string& host) {
  // Show whitelisted popups immediately.
  bool whitelisted = !!whitelist_.count(host);
  if (whitelisted)
    owner_->AddNewContents(tab_contents, NEW_POPUP, bounds, true, GURL());

  if (has_been_dismissed_) {
    // Don't want to show any other UI.
    if (!whitelisted)
      delete tab_contents;  // Discard blocked popups entirely.
    return;
  }

  if (whitelisted) {
    // Listen for this popup's destruction, so if the user closes it manually,
    // we'll know to stop caring about it.
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));

    unblocked_popups_[tab_contents] = host;
  } else {
    if (blocked_popups_.size() >= kImpossibleNumberOfPopups) {
      delete tab_contents;
      LOG(INFO) << "Warning: Renderer is sending more popups to us than should "
          "be possible. Renderer compromised?";
      return;
    }
    blocked_popups_.push_back(BlockedPopup(tab_contents, bounds, host));

    tab_contents->set_delegate(this);
  }

  PopupHosts::const_iterator i(popup_hosts_.find(host));
  if (i == popup_hosts_.end())
    popup_hosts_[host] = whitelisted;
  else
    DCHECK_EQ(whitelisted, i->second);

  // Update UI.
  container_view_->UpdateLabel();
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  if (!Animation::IsAnimating() && visibility_percentage_ < 1.0) {
    in_show_animation_ = true;
    Animation::SetDuration(kShowAnimationDurationMS);
    Animation::Start();
  }
  owner_->PopupNotificationVisibilityChanged(true);
}

void BlockedPopupContainer::LaunchPopupAtIndex(size_t index) {
  if (index >= blocked_popups_.size())
    return;

  // Open the popup.
  BlockedPopups::iterator i(blocked_popups_.begin() + index);
  TabContents* tab_contents = i->tab_contents;
  tab_contents->set_delegate(NULL);
  owner_->AddNewContents(tab_contents, NEW_POPUP, i->bounds, true, GURL());

  const std::string& host = i->host;
  if (!host.empty()) {
    // Listen for this popup's destruction, so if the user closes it manually,
    // we'll know to stop caring about it.
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));

    // Add the popup to the unblocked list.  (Do this before the below call!)
    unblocked_popups_[tab_contents] = i->host;
  }

  // Remove the popup from the blocked list.
  EraseDataForPopupAndUpdateUI(i);
}

size_t BlockedPopupContainer::GetBlockedPopupCount() const {
  return blocked_popups_.size();
}

void BlockedPopupContainer::GetURLAndTitleForPopup(size_t index,
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

bool BlockedPopupContainer::IsHostWhitelisted(size_t index) const {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  return (i == popup_hosts_.end()) ? false : i->second;
}

void BlockedPopupContainer::ToggleWhitelistingForHost(size_t index) {
  PopupHosts::const_iterator i(ConvertHostIndexToIterator(index));
  const std::string& host = i->first;
  bool should_whitelist = !i->second;
  popup_hosts_[host] = should_whitelist;

  ListValue* whitelist_pref =
      prefs_->GetMutableList(prefs::kPopupWhitelistedHosts);
  if (should_whitelist) {
    whitelist_.insert(host);
    whitelist_pref->Append(new StringValue(host));

    // Open the popups in order.
    for (size_t j = 0; j < blocked_popups_.size(); ) {
      if (blocked_popups_[j].host == host)
        LaunchPopupAtIndex(j);  // This shifts the rest of the entries down.
      else
        ++j;
    }
  } else {
    // Remove from whitelist.
    whitelist_.erase(host);
    whitelist_pref->Remove(StringValue(host));

    for (UnblockedPopups::iterator i(unblocked_popups_.begin());
         i != unblocked_popups_.end(); ) {
      TabContents* tab_contents = i->first;
      TabContentsDelegate* delegate = tab_contents->delegate();
      if ((i->second == host) && delegate->IsPopup(tab_contents)) {
        // Convert the popup back into a blocked popup.
        delegate->DetachContents(tab_contents);
        tab_contents->set_delegate(this);

        // Add the popup to the blocked list.  (Do this before the below call!)
        gfx::Rect bounds;
        tab_contents->GetContainerBounds(&bounds);
        blocked_popups_.push_back(BlockedPopup(tab_contents, bounds, host));

        // Remove the popup from the unblocked list.
        i = EraseDataForPopupAndUpdateUI(i);
      } else {
        ++i;
      }
    }
  }
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
    TabContents* tab_contents = it->tab_contents;
    if (tab_contents == source) {
      tab_contents->set_delegate(NULL);
      EraseDataForPopupAndUpdateUI(it);
      delete tab_contents;
      break;
    }
  }
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

// private:

BlockedPopupContainer::BlockedPopupContainer(TabContents* owner,
                                             PrefService* prefs)
    : Animation(kFramerate, NULL),
      owner_(owner),
      prefs_(prefs),
      container_view_(NULL),
      has_been_dismissed_(false),
      in_show_animation_(false),
      visibility_percentage_(0) {
  // Copy whitelist pref into local member that's easier to use.
  const ListValue* whitelist_pref =
      prefs_->GetList(prefs::kPopupWhitelistedHosts);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (whitelist_pref != NULL) {
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      whitelist_.insert(host);
    }
  }
}

void BlockedPopupContainer::AnimateToState(double state) {
  visibility_percentage_ = in_show_animation_ ? state : (1 - state);
  SetPosition();
}

void BlockedPopupContainer::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  TabContents* tab_contents = Source<TabContents>(source).ptr();
  UnblockedPopups::iterator i(unblocked_popups_.find(tab_contents));
  DCHECK(i != unblocked_popups_.end());
  EraseDataForPopupAndUpdateUI(i);
}

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

void BlockedPopupContainer::Init(const gfx::Point& initial_anchor) {
  container_view_ = new BlockedPopupContainerView(this);
  container_view_->SetVisible(true);

  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  WidgetWin::Init(owner_->GetNativeView(), gfx::Rect(), false);
  SetContentsView(container_view_);
  RepositionConstrainedWindowTo(initial_anchor);
}

void BlockedPopupContainer::HideSelf() {
  in_show_animation_ = false;
  Animation::SetDuration(kHideAnimationDurationMS);
  Animation::Start();
  owner_->PopupNotificationVisibilityChanged(false);
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

  if (real_height > 0) {
    int x;
    if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) {
      // Size this window using the anchor point as top-right corner.
      x = base_x;
    } else {
      // Size this window to the bottom left corner of top client window. In
      // Chrome, scrollbars always appear on the right, even for a RTL page or
      // when the UI is RTL (see http://crbug.com/6113 for more detail). Thus 0
      // is always a safe value for x-axis.
      x = 0;
    }
    SetWindowPos(HWND_TOP, x, real_y, size.width(), real_height, 0);
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

  registrar_.RemoveAll();
  unblocked_popups_.clear();

  popup_hosts_.clear();
}

BlockedPopupContainer::PopupHosts::const_iterator
    BlockedPopupContainer::ConvertHostIndexToIterator(size_t index) const {
  if (index >= popup_hosts_.size())
    return popup_hosts_.end();
  // If only there was a std::map::const_iterator::operator +=() ...
  PopupHosts::const_iterator i(popup_hosts_.begin());
  for (size_t j = 0; j < index; ++j)
    ++i;
  return i;
}

void BlockedPopupContainer::EraseDataForPopupAndUpdateUI(
    BlockedPopups::iterator i) {
  // Erase the host if this is the last popup for that host.
  const std::string& host = i->host;
  if (!host.empty()) {
    bool should_erase_host = true;
    for (BlockedPopups::const_iterator j(blocked_popups_.begin());
         j != blocked_popups_.end(); ++j) {
      if ((j != i) && (j->host == host)) {
        should_erase_host = false;
        break;
      }
    }
    if (should_erase_host) {
      for (UnblockedPopups::const_iterator j(unblocked_popups_.begin());
           j != unblocked_popups_.end(); ++j) {
        if (j->second == host) {
          should_erase_host = false;
          break;
        }
      }
      if (should_erase_host)
        popup_hosts_.erase(host);
    }
  }

  // Erase the popup and update the UI.
  blocked_popups_.erase(i);
  if (blocked_popups_.empty() && unblocked_popups_.empty())
    HideSelf();
  else
    container_view_->UpdateLabel();
}

BlockedPopupContainer::UnblockedPopups::iterator
    BlockedPopupContainer::EraseDataForPopupAndUpdateUI(
    UnblockedPopups::iterator i) {
  // Stop listening for this popup's destruction.
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(i->first));

  // Erase the host if this is the last popup for that host.
  const std::string& host = i->second;
  if (!host.empty()) {
    bool should_erase_host = true;
    for (UnblockedPopups::const_iterator j(unblocked_popups_.begin());
         j != unblocked_popups_.end(); ++j) {
      if ((j != i) && (j->second == host)) {
        should_erase_host = false;
        break;
      }
    }
    if (should_erase_host) {
      for (BlockedPopups::const_iterator j(blocked_popups_.begin());
           j != blocked_popups_.end(); ++j) {
        if (j->host == host) {
          should_erase_host = false;
          break;
        }
      }
      if (should_erase_host)
        popup_hosts_.erase(host);
    }
  }

  // Erase the popup and update the UI.
  UnblockedPopups::iterator next_popup = unblocked_popups_.erase(i);
  if (blocked_popups_.empty() && unblocked_popups_.empty())
    HideSelf();
  else
    container_view_->UpdateLabel();
  return next_popup;
}
