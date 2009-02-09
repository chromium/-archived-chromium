// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_window_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/browser.h"

namespace {

gboolean MainWindowDestroyed(GtkWindow* window, BrowserWindowGtk* browser_win) {
  delete browser_win;
  return FALSE;  // Don't stop this message.
}

}  // namespace

BrowserWindowGtk::BrowserWindowGtk(Browser* browser) : browser_(browser) {
  Init();
}

BrowserWindowGtk::~BrowserWindowGtk() {
  Close();
}

void BrowserWindowGtk::Init() {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window_, "Chromium");
  gtk_window_set_default_size(window_, 640, 480);
  g_signal_connect(G_OBJECT(window_), "destroy",
                   G_CALLBACK(MainWindowDestroyed), this);
}

void BrowserWindowGtk::Show() {
  gtk_widget_show_all(GTK_WIDGET(window_));
}

void BrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::Close() {
  if (!window_)
    return;

  gtk_widget_destroy(GTK_WIDGET(window_));
  window_ = NULL;
}

void BrowserWindowGtk::Activate() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::FlashFrame() {
  NOTIMPLEMENTED();
}

void* BrowserWindowGtk::GetNativeHandle() {
  NOTIMPLEMENTED();
  return NULL;
}

BrowserWindowTesting* BrowserWindowGtk::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* BrowserWindowGtk::GetStatusBubble() {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowGtk::SelectedTabToolbarSizeChanged(bool is_animating) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateTitleBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateLoadingAnimations(bool should_animate) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::SetStarredState(bool is_starred) {
  NOTIMPLEMENTED();
}

gfx::Rect BrowserWindowGtk::GetNormalBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool BrowserWindowGtk::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowGtk::UpdateStopGoState(bool is_loading) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateToolbar(TabContents* contents,
                                     bool should_restore_state) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect BrowserWindowGtk::GetRootWindowResizerRect() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void BrowserWindowGtk::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowBookmarkManager() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void BrowserWindowGtk::ShowBookmarkBubble(const GURL& url,
                                          bool already_bookmarked) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowImportDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowSearchEnginesDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                                      void* parent_window) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::DestroyBrowser() {
  browser_.reset();
}

