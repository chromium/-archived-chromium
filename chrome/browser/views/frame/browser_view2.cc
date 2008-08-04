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

#include "chrome/browser/views/frame/browser_view2.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents_container_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/l10n_util.h"
#include "generated_resources.h"

// static
static const int kToolbarTabStripVerticalOverlap = 3;
static const int kStatusBubbleHeight = 20;
static const int kStatusBubbleOffset = 2;

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, public:

BrowserView2::BrowserView2(Browser* browser)
    : ClientView(NULL, NULL),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      active_info_bar_(NULL),
      active_download_shelf_(NULL),
      toolbar_(NULL),
      contents_container_(NULL),
      initialized_(false) {
}

BrowserView2::~BrowserView2() {
}

gfx::Rect BrowserView2::GetToolbarBounds() const {
  CRect bounds;
  toolbar_->GetBounds(&bounds);
  return gfx::Rect(bounds);
}

gfx::Rect BrowserView2::GetClientAreaBounds() const {
  CRect bounds;
  contents_container_->GetBounds(&bounds);
  bounds.OffsetRect(GetX(), GetY());
  return gfx::Rect(bounds);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, BrowserWindow implementation:

void BrowserView2::Init() {
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  toolbar_ = new BrowserToolbarView(browser_->controller(), browser_.get());
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  contents_container_ = new TabContentsContainerView;
  set_contents_view(contents_container_);
  AddChildView(contents_container_);

  status_bubble_.reset(new StatusBubble(GetViewContainer()));
}

void BrowserView2::Show(int command, bool adjust_to_fit) {
  frame_->GetWindow()->Show();
}

void BrowserView2::BrowserDidPaint(HRGN region) {
}

void BrowserView2::Close() {
  frame_->GetWindow()->Close();
}

void* BrowserView2::GetPlatformID() {
  return GetViewContainer()->GetHWND();
}

TabStrip* BrowserView2::GetTabStrip() const {
  return NULL;
}

StatusBubble* BrowserView2::GetStatusBubble() {
  return status_bubble_.get();
}

ChromeViews::RootView* BrowserView2::GetRootView() {
  // TODO(beng): get rid of this stupid method.
  return View::GetRootView();
}

void BrowserView2::ShelfVisibilityChanged() {
  UpdateUIForContents(browser_->GetSelectedTabContents());
}

void BrowserView2::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    contents_container_->set_fast_resize(true);
    ShelfVisibilityChanged();
    contents_container_->set_fast_resize(false);
  } else {
    ShelfVisibilityChanged();
    contents_container_->UpdateHWNDBounds();
  }
}

void BrowserView2::UpdateTitleBar() {
}

void BrowserView2::SetWindowTitle(const std::wstring& title) {
}

void BrowserView2::Activate() {
}

void BrowserView2::FlashFrame() {
}

void BrowserView2::ShowTabContents(TabContents* contents) {
  contents_container_->SetTabContents(contents);

  // Force a LoadingStateChanged notification because the TabContents
  // could be loading (such as when the user unconstrains a tab.
  if (contents && contents->delegate())
    contents->delegate()->LoadingStateChanged(contents);

  UpdateUIForContents(contents);
}

void BrowserView2::ContinueDetachConstrainedWindowDrag(
    const gfx::Point& mouse_pt,
    int frame_component) {
}

void BrowserView2::SizeToContents(const gfx::Rect& contents_bounds) {
}

void BrowserView2::SetAcceleratorTable(
    std::map<ChromeViews::Accelerator, int>* accelerator_table) {
}

void BrowserView2::ValidateThrobber() {
}

gfx::Rect BrowserView2::GetNormalBounds() {
  return gfx::Rect();
}

bool BrowserView2::IsMaximized() {
  return false;
}

gfx::Rect BrowserView2::GetBoundsForContentBounds(
    const gfx::Rect content_rect) {
  return gfx::Rect();
}

void BrowserView2::DetachFromBrowser() {
}

void BrowserView2::InfoBubbleShowing() {
}

void BrowserView2::InfoBubbleClosing() {
}

ToolbarStarToggle* BrowserView2::GetStarButton() const {
  return toolbar_->star_button();
}

LocationBarView* BrowserView2::GetLocationBarView() const {
  return toolbar_->GetLocationBarView();
}

GoButton* BrowserView2::GetGoButton() const {
  return toolbar_->GetGoButton();
}

BookmarkBarView* BrowserView2::GetBookmarkBarView() {
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

BrowserView* BrowserView2::GetBrowserView() const {
  return NULL;
}

void BrowserView2::Update(TabContents* contents, bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView2::ProfileChanged(Profile* profile) {
  toolbar_->SetProfile(profile);
}

void BrowserView2::FocusToolbar() {
  toolbar_->RequestFocus();
}

void BrowserView2::DestroyBrowser() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::WindowDelegate implementation:

bool BrowserView2::CanResize() const {
  return true;
}

bool BrowserView2::CanMaximize() const {
  return true;
}

bool BrowserView2::IsModal() const {
  return false;
}

std::wstring BrowserView2::GetWindowTitle() const {
  return L"Magic browzR";
}

ChromeViews::View* BrowserView2::GetInitiallyFocusedView() const {
  return NULL;
}

bool BrowserView2::ShouldShowWindowTitle() const {
  return false;
}

SkBitmap BrowserView2::GetWindowIcon() {
  return SkBitmap();
}

bool BrowserView2::ShouldShowWindowIcon() const {
  return false;
}

void BrowserView2::ExecuteWindowsCommand(int command_id) {
  if (browser_->SupportsCommand(command_id) &&
      browser_->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
  }
}

void BrowserView2::WindowClosing() {
}

ChromeViews::View* BrowserView2::GetContentsView() {
  return NULL;
}

ChromeViews::ClientView* BrowserView2::CreateClientView(
    ChromeViews::Window* window) {
  set_window(window);
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::ClientView overrides:

bool BrowserView2::CanClose() const {
  return true;
}

int BrowserView2::NonClientHitTest(const gfx::Point& point) {
  return HTNOWHERE;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::View overrides:

void BrowserView2::Layout() {
  int top = LayoutTabStrip();
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutDownloadShelf();
  LayoutTabContents(top, bottom);
  LayoutStatusBubble(bottom);
  SchedulePaint();
}

void BrowserView2::DidChangeBounds(const CRect& previous,
                                   const CRect& current) {
  Layout();
}

void BrowserView2::ViewHierarchyChanged(bool is_add,
                                        ChromeViews::View* parent,
                                        ChromeViews::View* child) {
  if (is_add && child == this && GetViewContainer() && !initialized_) {
    Init();
    initialized_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, private:

int BrowserView2::LayoutTabStrip() {
  return 40; // TODO(beng): hook this up.
}

int BrowserView2::LayoutToolbar(int top) {
  CSize ps;
  toolbar_->GetPreferredSize(&ps);
  int toolbar_y = top - kToolbarTabStripVerticalOverlap;
  toolbar_->SetBounds(0, toolbar_y, GetWidth(), ps.cy);
  return toolbar_y + ps.cy;
  // TODO(beng): support toolbar-less windows.
}

int BrowserView2::LayoutBookmarkAndInfoBars(int top) {
  // If we have an Info-bar showing, and we're showing the New Tab Page, and
  // the Bookmark bar isn't visible on all tabs, then we need to show the Info
  // bar _above_ the Bookmark bar, since the Bookmark bar is styled to look
  // like it's part of the New Tab Page...
  if (active_info_bar_ && active_bookmark_bar_ &&
      bookmark_bar_view_->IsNewTabPage() &&
      !bookmark_bar_view_->IsAlwaysShown()) {
    top = LayoutInfoBar(top);
    return LayoutBookmarkBar(top);
  }
  // Otherwise, Bookmark bar first, Info bar second.
  top = LayoutBookmarkBar(top);
  return LayoutInfoBar(top);
}

int BrowserView2::LayoutBookmarkBar(int top) {
  if (active_bookmark_bar_) {
    CSize ps;
    active_bookmark_bar_->GetPreferredSize(&ps);
    active_bookmark_bar_->SetBounds(0, top, GetWidth(), ps.cy);
    top += ps.cy;
  }  
  return top;
}
int BrowserView2::LayoutInfoBar(int top) {
  if (active_info_bar_) {
    CSize ps;
    active_info_bar_->GetPreferredSize(&ps);
    active_info_bar_->SetBounds(0, top, GetWidth(), ps.cy);
    top += ps.cy;
  }
  return top;
}

void BrowserView2::LayoutTabContents(int top, int bottom) {
  contents_container_->SetBounds(0, top, GetWidth(), bottom - top);
}

int BrowserView2::LayoutDownloadShelf() {
  int bottom = GetHeight();
  if (active_download_shelf_) {
    CSize ps;
    active_download_shelf_->GetPreferredSize(&ps);
    active_download_shelf_->SetBounds(0, bottom - ps.cy, GetWidth(), ps.cy);
    bottom -= ps.cy;
  }
  return bottom;
}

void BrowserView2::LayoutStatusBubble(int top) {
  int status_bubble_y =
      top - kStatusBubbleHeight + kStatusBubbleOffset + GetY();
  status_bubble_->SetBounds(kStatusBubbleOffset, status_bubble_y,
                            GetWidth() / 3, kStatusBubbleHeight);
}

void BrowserView2::UpdateUIForContents(TabContents* contents) {
  // Coalesce layouts.
  bool changed = false;

  ChromeViews::View* new_shelf = NULL;
  if (contents && contents->IsDownloadShelfVisible())
    new_shelf = contents->GetDownloadShelfView();
  changed |= UpdateChildViewAndLayout(new_shelf, &active_download_shelf_);

  ChromeViews::View* new_info_bar = NULL;
  if (contents && contents->IsInfoBarVisible())
    new_info_bar = contents->GetInfoBarView();
  changed |= UpdateChildViewAndLayout(new_info_bar, &active_info_bar_);

  ChromeViews::View* new_bookmark_bar = NULL;
  if (contents) // TODO(beng): check for support of BookmarkBar
    new_bookmark_bar = GetBookmarkBarView();
  changed |= UpdateChildViewAndLayout(new_bookmark_bar, &active_bookmark_bar_);

  // Only do a Layout if the current contents is non-NULL. We assume that if
  // the contents is NULL, we're either being destroyed, or ShowTabContents is
  // going to be invoked with a non-NULL TabContents again so that there is no
  // need to do a Layout now.
  if (changed && contents)
    Layout();
}

bool BrowserView2::UpdateChildViewAndLayout(ChromeViews::View* new_view,
                                            ChromeViews::View** old_view) {
  DCHECK(old_view);
  if (*old_view == new_view) {
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
  if (*old_view) {
    current_height = (*old_view)->GetHeight();
    RemoveChildView(*old_view);
  }

  int new_height = 0;
  if (new_view) {
    CSize preferred_size;
    new_view->GetPreferredSize(&preferred_size);
    new_height = preferred_size.cy;
    AddChildView(new_view);
  }
  bool changed = false;
  if (new_height != current_height) {
    changed = true;
  } else if (new_view && *old_view) {
    // The view changed, but the new view wants the same size, give it the
    // bounds of the last view and have it repaint.
    CRect last_bounds;
    (*old_view)->GetBounds(&last_bounds);
    new_view->SetBounds(last_bounds.left, last_bounds.top,
                        last_bounds.Width(), last_bounds.Height());
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
