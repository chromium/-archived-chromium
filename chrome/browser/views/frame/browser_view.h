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
/*                  public ChromeViews::ClientView */
                    public ChromeViews::View {
 public:
  BrowserView(BrowserWindow* frame,
              Browser* browser,
              ChromeViews::Window* window,
              ChromeViews::View* contents_view);
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

  /*
  // Overridden from ChromeViews::ClientView:
  virtual bool CanClose() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  */

  // Overridden from ChromeViews::View:
  virtual void Layout();
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

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
