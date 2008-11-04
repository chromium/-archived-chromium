// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_view.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/l10n_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// Status Bubble metrics.
static const int kStatusBubbleHeight = 20;
static const int kStatusBubbleHorizontalOffset = 3;
static const int kStatusBubbleVerticalOffset = 2;

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

BrowserView::BrowserView(BrowserWindow* frame,
                         Browser* browser,
                         views::Window* window,
                         views::View* contents_view)
    : frame_(frame),
      browser_(browser),
      initialized_(false)
/*                     ,
      ClientView(window, contents_view) */ {
}

BrowserView::~BrowserView() {
}

void BrowserView::LayoutStatusBubble(int status_bubble_y) {
  status_bubble_->SetBounds(kStatusBubbleHorizontalOffset,
                            status_bubble_y - kStatusBubbleHeight +
                            kStatusBubbleVerticalOffset,
                            width() / 3,
                            kStatusBubbleHeight);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Init() {
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  toolbar_ = new BrowserToolbarView(browser_->controller(), browser_);
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  status_bubble_.reset(new StatusBubble(GetContainer()));
}

void BrowserView::Show(int command, bool adjust_to_fit) {
  frame_->Show(command, adjust_to_fit);
}

void BrowserView::Close() {
  frame_->Close();
}

void* BrowserView::GetPlatformID() {
  return frame_->GetPlatformID();
}

TabStrip* BrowserView::GetTabStrip() const {
  return frame_->GetTabStrip();
}

StatusBubble* BrowserView::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserView::ShelfVisibilityChanged() {
  frame_->ShelfVisibilityChanged();
}

void BrowserView::SelectedTabToolbarSizeChanged(bool is_animating) {
  frame_->SelectedTabToolbarSizeChanged(is_animating);
}

void BrowserView::UpdateTitleBar() {
  frame_->UpdateTitleBar();
}

void BrowserView::SetWindowTitle(const std::wstring& title) {
  frame_->SetWindowTitle(title);
}

void BrowserView::Activate() {
  frame_->Activate();
}

void BrowserView::FlashFrame() {
  frame_->FlashFrame();
}

void BrowserView::ShowTabContents(TabContents* contents) {
  frame_->ShowTabContents(contents);
}

void BrowserView::SizeToContents(const gfx::Rect& contents_bounds) {
  frame_->SizeToContents(contents_bounds);
}

void BrowserView::SetAcceleratorTable(
    std::map<views::Accelerator, int>* accelerator_table) {
  frame_->SetAcceleratorTable(accelerator_table);
}

void BrowserView::ValidateThrobber() {
  frame_->ValidateThrobber();
}

gfx::Rect BrowserView::GetNormalBounds() {
  return frame_->GetNormalBounds();
}

bool BrowserView::IsMaximized() {
  return frame_->IsMaximized();
}

gfx::Rect BrowserView::GetBoundsForContentBounds(const gfx::Rect content_rect) {
  return frame_->GetBoundsForContentBounds(content_rect);
}

void BrowserView::InfoBubbleShowing() {
  frame_->InfoBubbleShowing();
}

void BrowserView::InfoBubbleClosing() {
  frame_->InfoBubbleClosing();
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
  return frame_->GetBookmarkBarView();
}

BrowserView* BrowserView::GetBrowserView() const {
  return NULL;
}

void BrowserView::UpdateToolbar(TabContents* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView::ProfileChanged(Profile* profile) {
  toolbar_->SetProfile(profile);
}

void BrowserView::FocusToolbar() {
  toolbar_->RequestFocus();
}

void BrowserView::DestroyBrowser() {
  frame_->DestroyBrowser();
}

bool BrowserView::IsBookmarkBarVisible() const {
  BookmarkBarView* bookmark_bar_view = frame_->GetBookmarkBarView();
  if (!bookmark_bar_view)
    return false;

  if (bookmark_bar_view->IsNewTabPage() || bookmark_bar_view->IsAnimating())
    return true;

  // 1 is the minimum in GetPreferredSize for the bookmark bar.
  return bookmark_bar_view->GetPreferredSize().height() > 1;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

/*
bool BrowserView::CanClose() const {
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  return HTCLIENT;
}
*/

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

void BrowserView::Layout() {
  toolbar_->SetBounds(0, 0, width(), height());
}

void BrowserView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this && GetContainer() && !initialized_) {
    Init();
    // Make sure not to call Init() twice if we get inserted into a different
    // Container.
    initialized_ = true;
  }
}
