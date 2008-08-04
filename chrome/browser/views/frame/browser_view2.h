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

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_

#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"

// NOTE: For more information about the objects and files in this directory,
//       view: https://sites.google.com/a/google.com/the-chrome-project/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserToolbarView;
class StatusBubble;
class TabContentsContainerView;

///////////////////////////////////////////////////////////////////////////////
// BrowserView2
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView2 : public BrowserWindow,
                     public ChromeViews::WindowDelegate,
                     public ChromeViews::ClientView {
 public:
  explicit BrowserView2(Browser* browser);
  virtual ~BrowserView2();

  void set_frame(BrowserFrame* frame) { frame_ = frame; }

  // Returns the bounds of the toolbar, in BrowserView2 coordinates.
  gfx::Rect GetToolbarBounds() const;

  // Returns the bounds of the content area, in the coordinates of the
  // BrowserView2's parent.
  gfx::Rect GetClientAreaBounds() const;

  // Overridden from BrowserWindow:
  virtual void Init();
  virtual void Show(int command, bool adjust_to_fit);
  virtual void BrowserDidPaint(HRGN region);
  virtual void Close();
  virtual void* GetPlatformID();
  virtual TabStrip* GetTabStrip() const;
  virtual StatusBubble* GetStatusBubble();
  virtual ChromeViews::RootView* GetRootView();
  virtual void ShelfVisibilityChanged();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void SetWindowTitle(const std::wstring& title);
  virtual void Activate();
  virtual void FlashFrame();
  virtual void ShowTabContents(TabContents* contents);
  virtual void ContinueDetachConstrainedWindowDrag(
      const gfx::Point& mouse_pt,
      int frame_component);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual void SetAcceleratorTable(
      std::map<ChromeViews::Accelerator, int>* accelerator_table);
  virtual void ValidateThrobber();
  virtual gfx::Rect GetNormalBounds();
  virtual bool IsMaximized();
  virtual gfx::Rect GetBoundsForContentBounds(const gfx::Rect content_rect);
  virtual void DetachFromBrowser();
  virtual void InfoBubbleShowing();
  virtual void InfoBubbleClosing();
  virtual ToolbarStarToggle* GetStarButton() const;
  virtual LocationBarView* GetLocationBarView() const;
  virtual GoButton* GetGoButton() const;
  virtual BookmarkBarView* GetBookmarkBarView();
  virtual BrowserView* GetBrowserView() const;
  virtual void Update(TabContents* contents, bool should_restore_state);
  virtual void ProfileChanged(Profile* profile);
  virtual void FocusToolbar();
  virtual void DestroyBrowser();

  // Overridden from ChromeViews::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual ChromeViews::View* GetInitiallyFocusedView() const;
  virtual bool ShouldShowWindowTitle() const;
  virtual SkBitmap GetWindowIcon();
  virtual bool ShouldShowWindowIcon() const;
  virtual void ExecuteWindowsCommand(int command_id);
  virtual void WindowClosing();
  virtual ChromeViews::View* GetContentsView();
  virtual ChromeViews::ClientView* CreateClientView(
      ChromeViews::Window* window);

  // Overridden from ChromeViews::ClientView:
  virtual bool CanClose() const;
  virtual int NonClientHitTest(const gfx::Point& point);

  // Overridden from ChromeViews::View:
  virtual void Layout();
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Layout the TabStrip, returns the coordinate of the bottom of the TabStrip,
  // for laying out subsequent controls.
  int LayoutTabStrip();
  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);
  // Layout the TabContents container, between the coordinates |top| and
  // |bottom|.
  void LayoutTabContents(int top, int bottom);
  // Layout the Download Shelf, returns the coordinate of the top of the\
  // control, for laying out the previous control.
  int LayoutDownloadShelf();
  // Layout the Status Bubble.
  void LayoutStatusBubble(int top);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|.
  void UpdateUIForContents(TabContents* contents);

  // Updates an optional child View, e.g. Bookmarks Bar, Info Bar, Download
  // Shelf. If |*old_view| differs from new_view, the old_view is removed and
  // the new_view is added. This is intended to be used when swapping in/out
  // child views that are referenced via a field.
  // Returns true if anything was changed, and a re-Layout is now required.
  bool UpdateChildViewAndLayout(ChromeViews::View* new_view,
                                ChromeViews::View** old_view);

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_;

  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // Tool/Info bars that we are currently showing. Used for layout.
  ChromeViews::View* active_bookmark_bar_;
  ChromeViews::View* active_info_bar_;
  ChromeViews::View* active_download_shelf_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  BrowserToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The view that contains the selected TabContents.
  TabContentsContainerView* contents_container_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubble> status_bubble_;

  // True if we have already been initialized.
  bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserView2);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_
