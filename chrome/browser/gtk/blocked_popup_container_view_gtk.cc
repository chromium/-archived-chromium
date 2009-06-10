// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Inner padding between the border and the text label.
const int kInternalTopBottomPadding = 1;
const int kInternalLeftRightPadding = 2;

// Size of the border painted in kBorderColor
const int kBorderPadding = 1;

// Color of the border.
const GdkColor kFrameBorderColor = GDK_COLOR_RGB(190, 205, 223);

// TODO(erg): The windows signal has a gradient on the background. I don't know
// how to do that so we just fill with the bottom color.
const GdkColor kBackgroundColorBottom = GDK_COLOR_RGB(219, 235, 255);

}  // namespace

// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewGtk(container);
}

BlockedPopupContainerViewGtk::~BlockedPopupContainerViewGtk() {
  container_.Destroy();
}

TabContentsViewGtk* BlockedPopupContainerViewGtk::ContainingView() {
  return static_cast<TabContentsViewGtk*>(
      model_->GetConstrainingContents(NULL)->view());
}

void BlockedPopupContainerViewGtk::GetURLAndTitleForPopup(
    size_t index, string16* url, string16* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = model_->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToUTF16(tab_contents_url.possibly_invalid_spec());
  *title = tab_contents->GetTitle();
}

// Overridden from BlockedPopupContainerView:
void BlockedPopupContainerViewGtk::SetPosition() {
  // No-op. Not required with the GTK version.
}

void BlockedPopupContainerViewGtk::ShowView() {
  // TODO(erg): Animate in.
  gtk_widget_show_all(container_.get());
}

void BlockedPopupContainerViewGtk::UpdateLabel() {
  size_t blocked_popups = model_->GetBlockedPopupCount();

  gtk_button_set_label(
      GTK_BUTTON(menu_button_),
      (blocked_popups > 0) ?
      l10n_util::GetStringFUTF8(IDS_POPUPS_BLOCKED_COUNT,
                                UintToString16(blocked_popups)).c_str() :
      l10n_util::GetStringUTF8(IDS_POPUPS_UNBLOCKED).c_str());
}

void BlockedPopupContainerViewGtk::HideView() {
  // TODO(erg): Animate out.
  gtk_widget_hide(container_.get());
}

void BlockedPopupContainerViewGtk::Destroy() {
  ContainingView()->RemoveBlockedPopupView(this);
  delete this;
}

bool BlockedPopupContainerViewGtk::IsCommandEnabled(int command_id) const {
  return true;
}

bool BlockedPopupContainerViewGtk::IsItemChecked(int id) const {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    return model_->IsHostWhitelisted(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  }

  return false;
}

void BlockedPopupContainerViewGtk::ExecuteCommand(int id) {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    model_->ToggleWhitelistingForHost(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  } else {
    model_->LaunchPopupAtIndex(id_size_t - 1);
  }
}

BlockedPopupContainerViewGtk::BlockedPopupContainerViewGtk(
    BlockedPopupContainer* container)
    : model_(container),
      close_button_(CustomDrawButton::CloseButton()) {
  Init();
}

void BlockedPopupContainerViewGtk::Init() {
  menu_button_ = gtk_chrome_button_new();
  UpdateLabel();
  g_signal_connect(menu_button_, "clicked",
                   G_CALLBACK(OnMenuButtonClicked), this);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), menu_button_, FALSE, FALSE, 0);
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClicked), this);

  GtkWidget* padding = gtk_util::CreateGtkBorderBin(
      hbox, &kBackgroundColorBottom,
      kInternalTopBottomPadding, kInternalTopBottomPadding,
      kInternalLeftRightPadding, kInternalLeftRightPadding);

  container_.Own(gtk_util::CreateGtkBorderBin(padding, &kFrameBorderColor,
      kBorderPadding, kBorderPadding, kBorderPadding, kBorderPadding));

  ContainingView()->AttachBlockedPopupView(this);
}

void BlockedPopupContainerViewGtk::OnMenuButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->launch_menu_.reset(new MenuGtk(container, false));

  // Set items 1 .. popup_count as individual popups.
  size_t popup_count = container->model_->GetBlockedPopupCount();
  for (size_t i = 0; i < popup_count; ++i) {
    string16 url, title;
    container->GetURLAndTitleForPopup(i, &url, &title);
    // We can't just use the index into container_ here because Menu reserves
    // the value 0 as the nop command.
    container->launch_menu_->AppendMenuItemWithLabel(i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_TITLE_FORMAT, url, title));
  }

  // Set items (kImpossibleNumberOfPopups + 1) ..
  // (kImpossibleNumberOfPopups + 1 + hosts.size()) as hosts.
  std::vector<std::string> hosts(container->model_->GetHosts());
  if (!hosts.empty() && (popup_count > 0))
    container->launch_menu_->AppendSeparator();
  for (size_t i = 0; i < hosts.size(); ++i) {
    container->launch_menu_->AppendCheckMenuItemWithLabel(
        BlockedPopupContainer::kImpossibleNumberOfPopups + i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_HOST_FORMAT,
                                  UTF8ToUTF16(hosts[i])));
  }

  container->launch_menu_->PopupAsContext(gtk_get_current_event_time());
}

void BlockedPopupContainerViewGtk::OnCloseButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->model_->set_dismissed();
  container->model_->CloseAll();
}

