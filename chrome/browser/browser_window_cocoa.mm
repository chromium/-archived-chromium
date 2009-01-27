// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "chrome/browser/browser_window_cocoa.h"
#include "chrome/browser/browser_window_controller.h"

BrowserWindowCocoa::BrowserWindowCocoa(BrowserWindowController* controller, 
                                       NSWindow* window)
    : controller_(controller), window_(window) {
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
}

void BrowserWindowCocoa::Init() {
}

void BrowserWindowCocoa::Show() {
  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  NSRect cocoa_bounds = NSMakeRect(bounds.x(), 0, bounds.width(),
                                   bounds.height());
  // flip coordinates
  NSScreen* screen = [window_ screen];
  cocoa_bounds.origin.y = 
      [screen frame].size.height - bounds.height() - bounds.y();
}

void BrowserWindowCocoa::Close() {
  [window_ orderOut:controller_];
}

void BrowserWindowCocoa::Activate() {
  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::FlashFrame() {
  [[NSApplication sharedApplication] 
      requestUserAttention:NSInformationalRequest];
}

void* BrowserWindowCocoa::GetNativeHandle() {
  return [controller_ window];
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return NULL;
}

void BrowserWindowCocoa::SelectedTabToolbarSizeChanged(bool is_animating) {
}

void BrowserWindowCocoa::UpdateTitleBar() {
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
}

gfx::Rect BrowserWindowCocoa::GetNormalBounds() const {
  // TODO(pinkerton): not sure if we can get the non-zoomed bounds, or if it
  // really matters. We may want to let Cocoa handle all this for us.
  NSRect frame = [window_ frame];
  NSScreen* screen = [window_ screen];
  gfx::Rect bounds(frame.origin.x, 0, frame.size.width, frame.size.height);
  bounds.set_y([screen frame].size.height + frame.size.height + frame.origin.y);
  return bounds;
}

bool BrowserWindowCocoa::IsMaximized() {
  return [window_ isZoomed];
}

LocationBarView* BrowserWindowCocoa::GetLocationBarView() const {
  return NULL;
}

BookmarkBarView* BrowserWindowCocoa::GetBookmarkBarView() {
  return NULL;
}

void BrowserWindowCocoa::UpdateStopGoState(bool is_loading) {
}

void BrowserWindowCocoa::UpdateToolbar(TabContents* contents,
                                       bool should_restore_state) {
}

void BrowserWindowCocoa::FocusToolbar() {
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  return true;
}

void BrowserWindowCocoa::ToggleBookmarkBar() {
}

void BrowserWindowCocoa::ShowAboutChromeDialog() {
}

void BrowserWindowCocoa::ShowBookmarkManager() {
}

bool BrowserWindowCocoa::IsBookmarkBubbleVisible() const {
  return false;
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
}

void BrowserWindowCocoa::ShowReportBugDialog() {
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
}

void BrowserWindowCocoa::ShowImportDialog() {
}

void BrowserWindowCocoa::ShowSearchEnginesDialog() {
}

void BrowserWindowCocoa::ShowPasswordManager() {
}

void BrowserWindowCocoa::ShowSelectProfileDialog() {
}

void BrowserWindowCocoa::ShowNewProfileDialog() {
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                        void* parent_window) {
}
                            
void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}
