// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/blocked_popup_container_view_win.h"

#include <math.h>
#include <windows.h>

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"

#include "views/controls/scrollbar/native_scroll_bar.h"

namespace {
// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

// The background color of the blocked popup notification.
const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

// The border color of the blocked popup notification. This is the same as the
// border around the inside of the tab contents.
const SkColor kBorderColor = SkColorSetRGB(190, 205, 223);

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

}  // namespace


// The view presented to the user notifying them of the number of popups
// blocked. This view should only be used inside of BlockedPopupContainer.
class BlockedPopupContainerInternalView : public views::View,
                                  public views::ButtonListener,
                                  public views::Menu::Delegate {
 public:
  explicit BlockedPopupContainerInternalView(
      BlockedPopupContainerViewWin* container);
  ~BlockedPopupContainerInternalView();

  // Sets the label on the menu button.
  void UpdateLabel();

  std::wstring label() const { return popup_count_label_->text(); }

  // Overridden from views::View:

  // Paints our border and background. (Does not paint children.)
  virtual void Paint(gfx::Canvas* canvas);
  // Sets positions of all child views.
  virtual void Layout();
  // Gets the desired size of the popup notification.
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from Menu::Delegate:

  // Displays the status of the "Show Blocked Popup Notification" item.
  virtual bool IsItemChecked(int id) const;
  // Called after user clicks a menu item.
  virtual void ExecuteCommand(int id);

 private:
  // Our owner and HWND parent.
  BlockedPopupContainerViewWin* container_;

  // The button which brings up the popup menu.
  views::MenuButton* popup_count_label_;

  // Our "X" button.
  views::ImageButton* close_button_;

  // Popup menu shown to user.
  scoped_ptr<views::Menu> launch_menu_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainerInternalView);
};


BlockedPopupContainerInternalView::BlockedPopupContainerInternalView(
    BlockedPopupContainerViewWin* container)
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

BlockedPopupContainerInternalView::~BlockedPopupContainerInternalView() {
}

void BlockedPopupContainerInternalView::UpdateLabel() {
  size_t blocked_popups = container_->GetBlockedPopupCount();
  popup_count_label_->SetText((blocked_popups > 0) ?
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT,
                            UintToWString(blocked_popups)) :
      l10n_util::GetString(IDS_POPUPS_UNBLOCKED));
  Layout();
  SchedulePaint();
}

void BlockedPopupContainerInternalView::Paint(gfx::Canvas* canvas) {
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

void BlockedPopupContainerInternalView::Layout() {
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

gfx::Size BlockedPopupContainerInternalView::GetPreferredSize() {
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

void BlockedPopupContainerInternalView::ButtonPressed(views::Button* sender) {
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
      launch_menu_->AppendMenuItem(
          BlockedPopupContainer::kImpossibleNumberOfPopups + i + 1,
          l10n_util::GetStringF(IDS_POPUP_HOST_FORMAT, hosts[i]),
          views::Menu::NORMAL);
    }

    CPoint cursor_position;
    ::GetCursorPos(&cursor_position);
    launch_menu_->RunMenuAt(cursor_position.x, cursor_position.y);
  } else if (sender == close_button_) {
    container_->GetModel()->set_dismissed();
    container_->GetModel()->CloseAll();
  }
}

bool BlockedPopupContainerInternalView::IsItemChecked(int id) const {
  if (id > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    return container_->GetModel()->IsHostWhitelisted(static_cast<size_t>(
        id - BlockedPopupContainer::kImpossibleNumberOfPopups - 1));
  }

  return false;
}

void BlockedPopupContainerInternalView::ExecuteCommand(int id) {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    container_->GetModel()->ToggleWhitelistingForHost(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  } else {
    container_->GetModel()->LaunchPopupAtIndex(id_size_t - 1);
  }
}


// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewWin(container);
}


BlockedPopupContainerViewWin::~BlockedPopupContainerViewWin() {
}

void BlockedPopupContainerViewWin::GetURLAndTitleForPopup(
    size_t index, std::wstring* url, std::wstring* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = GetModel()->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToWide(tab_contents_url.possibly_invalid_spec());
  *title = UTF16ToWideHack(tab_contents->GetTitle());
}

std::vector<std::wstring> BlockedPopupContainerViewWin::GetHosts() const {
  std::vector<std::string> utf8_hosts(GetModel()->GetHosts());

  std::vector<std::wstring> hosts;
  for (std::vector<std::string>::const_iterator it = utf8_hosts.begin();
       it != utf8_hosts.end(); ++it)
    hosts.push_back(UTF8ToWide(*it));
  return hosts;
}

size_t BlockedPopupContainerViewWin::GetBlockedPopupCount() const {
  return container_model_->GetBlockedPopupCount();
}

// Overridden from AnimationDelegate:

void BlockedPopupContainerViewWin::AnimationStarted(
    const Animation* animation) {
  SetPosition();
}

void BlockedPopupContainerViewWin::AnimationEnded(const Animation* animation) {
  SetPosition();
}

void BlockedPopupContainerViewWin::AnimationProgressed(
    const Animation* animation) {
  SetPosition();
}

// Overridden from BlockedPopupContainerView:

void BlockedPopupContainerViewWin::SetPosition() {
  // Get our parent's rect and size ourselves inside of it.
  HWND parent = GetParent();
  CRect client_rect;
  ::GetClientRect(parent, &client_rect);

  // TODO(erg): There's no way to detect whether scroll bars are
  // visible, so for beta, we're just going to assume that the
  // vertical scroll bar is visible, and not care about covering up
  // the horizontal scroll bar. Fixing this is half of
  // http://b/1118139.
  gfx::Point anchor_point(
      client_rect.Width() -
          views::NativeScrollBar::GetVerticalScrollBarWidth(),
      client_rect.Height());

  gfx::Size size = container_view_->GetPreferredSize();
  int base_x = anchor_point.x() - size.width();
  int base_y = anchor_point.y() - size.height();

  int real_height =
      static_cast<int>(size.height() * slide_animation_->GetCurrentValue());
  int real_y = anchor_point.y() - real_height;

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

void BlockedPopupContainerViewWin::ShowView() {
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  slide_animation_->Show();
}

void BlockedPopupContainerViewWin::UpdateLabel() {
  container_view_->UpdateLabel();
}

void BlockedPopupContainerViewWin::HideView() {
  slide_animation_->Hide();
}

void BlockedPopupContainerViewWin::Destroy() {
  Close();
}

// private:

BlockedPopupContainerViewWin::BlockedPopupContainerViewWin(
    BlockedPopupContainer* container)
    : slide_animation_(new SlideAnimation(this)),
      container_model_(container),
      container_view_(NULL) {
  container_view_ = new BlockedPopupContainerInternalView(this);
  container_view_->SetVisible(true);

  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  WidgetWin::Init(GetModel()->GetConstrainingContents(NULL)->GetNativeView(),
                  gfx::Rect());
  SetContentsView(container_view_);
  SetPosition();
}

void BlockedPopupContainerViewWin::OnSize(UINT param, const CSize& size) {
  // Set the window region so we have rounded corners on the top.
  SkRect rect;
  rect.set(0, 0, SkIntToScalar(size.cx), SkIntToScalar(size.cy));
  gfx::Path path;
  path.addRoundRect(rect, kRoundedCornerRad, SkPath::kCW_Direction);
  SetWindowRgn(path.CreateHRGN(), TRUE);

  ChangeSize(param, size);
}
