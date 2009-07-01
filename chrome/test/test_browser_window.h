// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_TEST_BROWSER_WINDOW_H_

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/test/test_location_bar.h"

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid TabStrip, all other getters return NULL.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  explicit TestBrowserWindow(Browser* browser)
      : tab_strip_(browser->tabstrip_model()) {
  }
  ~TestBrowserWindow() {}

  virtual void Init() {}
  virtual void Show() {}
  virtual void SetBounds(const gfx::Rect& bounds) {}
  virtual void Close() {}
  virtual void Activate() {}
  virtual bool IsActive() const { return false; }
  virtual void FlashFrame() {}
  virtual gfx::NativeWindow GetNativeHandle() { return NULL; }
  virtual BrowserWindowTesting* GetBrowserWindowTesting() { return NULL; }
  virtual StatusBubble* GetStatusBubble() { return NULL; }
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) {}
  virtual void UpdateTitleBar() {}
  virtual void UpdateDevTools() {}
  virtual void UpdateLoadingAnimations(bool should_animate) {}
  virtual void SetStarredState(bool is_starred) {}
  virtual gfx::Rect GetNormalBounds() const { return gfx::Rect(); }
  virtual bool IsMaximized() const { return false; }
  virtual void SetFullscreen(bool fullscreen) {}
  virtual bool IsFullscreen() const { return false; }
  virtual LocationBar* GetLocationBar() const {
    return const_cast<TestLocationBar*>(&location_bar_);
  }
  virtual void SetFocusToLocationBar() {}
  virtual void UpdateStopGoState(bool is_loading, bool force) {}
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state) {}
  virtual void FocusToolbar() {}
  virtual bool IsBookmarkBarVisible() const { return false; }
  virtual gfx::Rect GetRootWindowResizerRect() const { return gfx::Rect(); }
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) {}
  virtual void ToggleBookmarkBar() {}
  virtual void ShowAboutChromeDialog() {}
  virtual void ShowTaskManager() {}
  virtual void ShowBookmarkManager() {}
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {}
  virtual bool IsDownloadShelfVisible() const { return false; }
  virtual DownloadShelf* GetDownloadShelf() { return NULL; }
  virtual void ShowReportBugDialog() {}
  virtual void ShowClearBrowsingDataDialog() {}
  virtual void ShowImportDialog() {}
  virtual void ShowSearchEnginesDialog() {}
  virtual void ShowPasswordManager() {}
  virtual void ShowSelectProfileDialog() {}
  virtual void ShowNewProfileDialog() {}
  virtual void ConfirmBrowserCloseWithPendingDownloads() {}
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) {}
  virtual void UserChangedTheme() {}
  virtual int GetExtraRenderViewHeight() const { return 0; }
  virtual void TabContentsFocused(TabContents* tab_contents) { }

 protected:
  virtual void DestroyBrowser() {}

 private:
  TabStrip tab_strip_;

  TestLocationBar location_bar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindow);
};

#endif  // CHROME_TEST_TEST_BROWSER_WINDOW_H_
