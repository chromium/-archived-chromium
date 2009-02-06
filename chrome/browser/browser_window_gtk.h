// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_WINDOW_GTK_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/browser_window.h"

typedef struct _GtkWindow GtkWindow;

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk : public BrowserWindow {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

  // Overridden from BrowserWindow
  virtual void Init();
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual void FlashFrame();
  virtual void* GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetNormalBounds() const;
  virtual bool IsMaximized();
  virtual LocationBar* GetLocationBar() const;
  virtual void UpdateStopGoState(bool is_loading);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowBookmarkManager();
  virtual bool IsBookmarkBubbleVisible() const;
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                              void* parent_window);

 protected:
  virtual void DestroyBrowser();
  GtkWindow* window_;
  scoped_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_WINDOW_GTK_H_

