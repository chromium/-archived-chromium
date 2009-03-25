// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/status_bubble_mac.h"

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller,
                                       NSWindow* window)
  : browser_(browser), controller_(controller), window_(window) {
  status_bubble_.reset(new StatusBubbleMac(window_));
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
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

bool BrowserWindowCocoa::IsActive() const {
  return [window_ isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeHandle() {
  return [controller_ window];
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserWindowCocoa::SelectedTabToolbarSizeChanged(bool is_animating) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateTitleBar() {
  // This is used on windows to update the favicon and title in the window
  // icon, which we don't use on the mac.
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  [controller_ updateLoadingAnimations:should_animate ? YES : NO];
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
  NSRect tabRect = [controller_ selectedTabGrowBoxRect];
  return gfx::Rect(NSRectToCGRect(tabRect));
}

LocationBar* BrowserWindowCocoa::GetLocationBar() const {
  return [controller_ locationBar];
}

void BrowserWindowCocoa::SetFocusToLocationBar() {
  [controller_ focusLocationBar];
}

void BrowserWindowCocoa::UpdateStopGoState(bool is_loading) {
  [controller_ setIsLoading:is_loading ? YES : NO];
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
  // Conversion from ObjC BOOL to C++ bool.
  return [controller_ isBookmarkBarVisible] ? true : false;
}

// This is a little awkward.  Internal to Chrome, V and C (in the MVC
// sense) tend to smear together.  Thus, we have a call chain of
// C(browser_window)-->
// V(me;right here)-->
// C(BrowserWindowController)-->
// C(TabStripController) --> ...
void BrowserWindowCocoa::ToggleBookmarkBar() {
  [controller_ toggleBookmarkBar];
}

void BrowserWindowCocoa::ShowFindBar() {
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

