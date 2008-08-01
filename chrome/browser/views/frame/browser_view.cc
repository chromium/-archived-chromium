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

#include "chrome/browser/views/frame/browser_view.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/toolbar_star_toggle.h"

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

BrowserView::BrowserView(ChromeViews::Window* window,
                         ChromeViews::View* contents_view)
    : ClientView(window, contents_view) {
}

BrowserView::~BrowserView() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Init() {

}

void BrowserView::Show(int command, bool adjust_to_fit) {

}

void BrowserView::BrowserDidPaint(HRGN region) {

}

void BrowserView::Close() {

}

void* BrowserView::GetPlatformID() {
  return NULL;
}

TabStrip* BrowserView::GetTabStrip() const {
  return NULL;
}

StatusBubble* BrowserView::GetStatusBubble() {
  return NULL;
}

ChromeViews::RootView* BrowserView::GetRootView() {
  return NULL;
}

void BrowserView::ShelfVisibilityChanged() {

}

void BrowserView::SelectedTabToolbarSizeChanged(bool is_animating) {

}

void BrowserView::UpdateTitleBar() {

}

void BrowserView::SetWindowTitle(const std::wstring& title) {

}

void BrowserView::Activate() {

}

void BrowserView::FlashFrame() {

}

void BrowserView::ShowTabContents(TabContents* contents) {

}

void BrowserView::ContinueDetachConstrainedWindowDrag(
    const gfx::Point& mouse_pt,
    int frame_component) {

}

void BrowserView::SizeToContents(const gfx::Rect& contents_bounds) {

}

void BrowserView::SetAcceleratorTable(
    std::map<ChromeViews::Accelerator, int>* accelerator_table) {

}

void BrowserView::ValidateThrobber() {

}

gfx::Rect BrowserView::GetNormalBounds() {
  return gfx::Rect();
}

bool BrowserView::IsMaximized() {
  return false;
}

gfx::Rect BrowserView::GetBoundsForContentBounds(const gfx::Rect content_rect) {
  return gfx::Rect();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {

}

void BrowserView::DetachFromBrowser() {

}

void BrowserView::InfoBubbleShowing() {

}

void BrowserView::InfoBubbleClosing() {

}

ToolbarStarToggle* BrowserView::GetStarButton() const {
  return NULL;
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return NULL;
}

GoButton* BrowserView::GetGoButton() const {
  return NULL;
}

BookmarkBarView* BrowserView::GetBookmarkBarView() {
  return NULL;
}

void BrowserView::Update(TabContents* contents, bool should_restore_state) {

}

void BrowserView::ProfileChanged(Profile* profile) {

}

void BrowserView::FocusToolbar() {

}

void BrowserView::DestroyBrowser() {

}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ChromeViews::ClientView overrides:

bool BrowserView::CanClose() const {
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  return HTCLIENT;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ChromeViews::View overrides:

void BrowserView::Layout() {

}

void BrowserView::ViewHierarchyChanged(bool is_add,
                                       ChromeViews::View* parent,
                                       ChromeViews::View* child) {


}
