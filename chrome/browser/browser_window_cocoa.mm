// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/logging.h"
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

// Callers assume that this doesn't immediately delete the Browser object.
// The controller implementing the window delegate methods called from
// |-performClose:| must take precautiions to ensure that.
void BrowserWindowCocoa::Close() {
  [window_ orderOut:controller_];
  [window_ performClose:controller_];
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
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
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

bool BrowserWindowCocoa::IsMaximized() const {
  return [window_ isZoomed];
}

void BrowserWindowCocoa::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool BrowserWindowCocoa::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect BrowserWindowCocoa::GetRootWindowResizerRect() const {
  // TODO(pinkerton): fill this in so scrollbars go in the correct places
  NOTIMPLEMENTED();
  return gfx::Rect();
}

LocationBar* BrowserWindowCocoa::GetLocationBar() const {
  return [controller_ locationBar];
}

void BrowserWindowCocoa::SetFocusToLocationBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateStopGoState(bool is_loading) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateToolbar(TabContents* contents,
                                       bool should_restore_state) {
  [controller_ updateToolbarWithContents:contents 
                      shouldRestoreState:should_restore_state ? YES : NO];
}

void BrowserWindowCocoa::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void BrowserWindowCocoa::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowBookmarkManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowImportDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowSearchEnginesDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                        void* parent_window) {
  NOTIMPLEMENTED();
}
                            
void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}
