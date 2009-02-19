// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model.h"

class BrowserToolbarGtk;
class NineBox;
class StatusBubbleGtk;
class TabContentsContainerGtk;

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk : public BrowserWindow,
                         public TabStripModelObserver {
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
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowBookmarkManager();
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

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabStripEmpty();

  void OnBoundsChanged(const gfx::Rect& bounds);
  void OnStateChanged(GdkWindowState state);

 protected:
  virtual void DestroyBrowser();
  GtkWindow* window_;
  GtkWidget* vbox_;

  scoped_ptr<Browser> browser_;

 private:
  // Change whether we're showing the custom blue frame.
  // Must be called once at startup.
  // Triggers relayout of the content.
  void SetCustomFrame(bool custom_frame);

  // Callback for when the "content area" vbox needs to be redrawn.
  // The content area includes the toolbar and web page but not the tab strip.
  // It has a soft gray border when we have a custom frame.
  static gboolean OnContentAreaExpose(GtkWidget* widget, GdkEventExpose* e,
                                      BrowserWindowGtk* window);

  gfx::Rect bounds_;
  GdkWindowState state_;

  // Theme graphics for the content area.
  scoped_ptr<NineBox> content_area_ninebox_;

  // Whether we're drawing the custom Chrome frame (including title bar).
  bool custom_frame_;

  // The object that manages all of the widgets in the toolbar.
  scoped_ptr<BrowserToolbarGtk> toolbar_;

  // The status bubble manager.  Always non-NULL.
  scoped_ptr<StatusBubbleGtk> status_bubble_;

  // A container that manages the GtkWidget*s that are the webpage display
  // (along with associated infobars, shelves, and other things that are part
  // of the content area).
  scoped_ptr<TabContentsContainerGtk> contents_container_;
};

#endif  // CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_

