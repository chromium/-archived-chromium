// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_

#include "chrome/browser/browser_window.h"
#include "chrome/views/client_view.h"

// NOTE: For more information about the objects and files in this directory,
//       view: https://sites.google.com/a/google.com/the-chrome-project/developers/design-documents/browser-window

class Browser;
class BrowserToolbarView;

///////////////////////////////////////////////////////////////////////////////
// BrowserView
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView : public BrowserWindow,
/*                  public views::ClientView */
                    public views::View {
 public:
  BrowserView(BrowserWindow* frame,
              Browser* browser,
              views::Window* window,
              views::View* contents_view);
  virtual ~BrowserView();

  // TODO(beng): remove this once all layout is done inside this object.
  // Layout the Status bubble relative to position.
  void LayoutStatusBubble(int status_bubble_y);

  // Overridden from BrowserWindow:
  virtual void Init();
  virtual void Show(int command, bool adjust_to_fit);
  virtual void Close();
  virtual void* GetPlatformID();
  virtual TabStrip* GetTabStrip() const;
  virtual StatusBubble* GetStatusBubble();
  virtual void ShelfVisibilityChanged();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void SetWindowTitle(const std::wstring& title);
  virtual void Activate();
  virtual void FlashFrame();
  virtual void ShowTabContents(TabContents* contents);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual void SetAcceleratorTable(
      std::map<views::Accelerator, int>* accelerator_table);
  virtual void ValidateThrobber();
  virtual gfx::Rect GetNormalBounds();
  virtual bool IsMaximized();
  virtual gfx::Rect GetBoundsForContentBounds(const gfx::Rect content_rect);
  virtual void InfoBubbleShowing();
  virtual void InfoBubbleClosing();
  virtual ToolbarStarToggle* GetStarButton() const;
  virtual LocationBarView* GetLocationBarView() const;
  virtual GoButton* GetGoButton() const;
  virtual BookmarkBarView* GetBookmarkBarView();
  virtual BrowserView* GetBrowserView() const;
  virtual void UpdateToolbar(TabContents* contents, bool should_restore_state);
  virtual void ProfileChanged(Profile* profile);
  virtual void FocusToolbar();
  virtual void DestroyBrowser();
  virtual bool IsBookmarkBarVisible() const;

  /*
  // Overridden from views::ClientView:
  virtual bool CanClose() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  */

  // Overridden from views::View:
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // The Browser object we are associated with.
  // TODO(beng): (Cleanup) this should become a scoped_ptr.
  Browser* browser_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  BrowserToolbarView* toolbar_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubble> status_bubble_;

  // Temporary pointer to containing BrowserWindow.
  // TODO(beng): convert this to a BrowserFrame*.
  BrowserWindow* frame_;

  // True if we have already been initialized.
  bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
