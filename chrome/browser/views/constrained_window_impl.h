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

#ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_
#define CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_

#include "base/gfx/rect.h"
#include "chrome/browser/constrained_window.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/views/custom_frame_window.h"

class ConstrainedTabContentsWindowDelegate;
class ConstrainedWindowAnimation;
class ConstrainedWindowNonClientView;
namespace ChromeViews {
class HWNDView;
class WindowDelegate;
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowImpl
//
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame.
//
class ConstrainedWindowImpl : public ConstrainedWindow,
                              public ChromeViews::CustomFrameWindow,
                              public TabContentsDelegate {
 public:
  virtual ~ConstrainedWindowImpl();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  TabContents* constrained_contents() const { return constrained_contents_; }
  // Returns the non-client view inside this Constrained Window.
  // NOTE: Defining the function body here would require pulling in the
  // declarations of ConstrainedWindowNonClientView, as well as all the classes
  // it depends on, from the .cc file; the benefit isn't worth it.
  ConstrainedWindowNonClientView* non_client_view();

  // Overridden from ConstrainedWindow:
  virtual void CloseConstrainedWindow();
  virtual void ActivateConstrainedWindow();
  virtual void RepositionConstrainedWindowTo(const gfx::Point& anchor_point);
  virtual bool IsSuppressedConstrainedWindow() const;
  virtual void WasHidden();
  virtual void DidBecomeSelected();
  virtual std::wstring GetWindowTitle() const;
  virtual void UpdateWindowTitle();
  virtual const gfx::Rect& GetCurrentBounds() const;

  // Overridden from PageNavigator (TabContentsDelegate's base interface):
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition,
                              const std::string& override_encoding);

  // Overridden from TabContentsDelegate:
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual TabContents* GetConstrainingContents(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool CanBlur() const { return false; }

  virtual void NavigateToPage(TabContents* source, const GURL& url,
                              PageTransition::Type transition);


  bool is_dialog() { return is_dialog_; }

  // Changes the visibility of the titlebar. |percentage| is a real
  // number ranged 0,1.
  void SetTitlebarVisibilityPercentage(double percentage);

  // Starts a ConstrainedWindowAnimation to slide in the titlebar of
  // this suppressed constrained popup window.
  void StartSuppressedAnimation();

 protected:
  // Windows message handlers:
  virtual void OnDestroy();
  virtual void OnFinalMessage(HWND window);
  virtual void OnGetMinMaxInfo(LPMINMAXINFO mm_info);
  virtual LRESULT OnMouseActivate(HWND window, UINT hittest_code, UINT message);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);

 private:
  friend class ConstrainedWindow;

  // Use the static factory methods on ConstrainedWindow to construct a
  // ConstrainedWindow.
  ConstrainedWindowImpl(TabContents* owner,
                        ChromeViews::WindowDelegate* window_delegate,
                        TabContents* constrained_contents);
  ConstrainedWindowImpl(TabContents* owner,
                        ChromeViews::WindowDelegate* window_delegate);
  void Init(TabContents* owner);

  // Called after changing either the anchor point or titlebar
  // visibility of a suppressed popup.
  //
  // @see RepositionConstrainedWindowTo
  // @see SetTitlebarVisibilityPercentage
  void ResizeConstrainedTitlebar();

  // Called to change the size of a constrained window. Moves the
  // window to the anchor point (taking titlebar visibility into
  // account) and sets the pop up size.
  void ResizeConstrainedWindow(int width, int height);

  // Initialize the Constrained Window as a Constrained Dialog containing a
  // ChromeViews::View client area.
  void InitAsDialog(const gfx::Rect& initial_bounds);

  // Builds the underlying HWND and window delegates for a newly
  // created popup window.
  //
  // We have to split the initialization process for a popup window in
  // two because we first need to initialize a proper window delegate
  // so that when we query for desired size, we get accurate data. If
  // we didn't do this, windows will initialize to being smaller then
  // the desired content size plus room for browser chrome.
  void InitWindowForContents(TabContents* constrained_contents,
                             ConstrainedTabContentsWindowDelegate* delegate);

  // Sets the initial bounds for a newly created popup window.
  //
  // This is the second part of the initialization process started
  // with InitWindowForContents. For the parameter initial_bounds to
  // have been calculated correctly, InitWindowForContents must have
  // been run first.
  void InitSizeForContents(const gfx::Rect& initial_bounds);

  // Returns true if the Constrained Window can be detached from its owner.
  bool CanDetach() const;

  // Detach the Constrained TabContents from its owner.
  void Detach();

  // Updates the portions of the UI as specified in |changed_flags|.
  void UpdateUI(unsigned int changed_flags);

  // Place and size the window, constraining to the bounds of the |owner_|.
  void SetWindowBounds(const gfx::Rect& bounds);

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // The TabContents constrained by |owner_|.
  TabContents* constrained_contents_;

  // True if focus should not be restored to whatever view was focused last
  // when this window is destroyed.
  bool focus_restoration_disabled_;

  // A default ChromeViews::WindowDelegate implementation for this window when
  // a TabContents is being constrained. (For the Constrained Dialog case, the
  // caller is required to provide the WindowDelegate).
  scoped_ptr<ChromeViews::WindowDelegate> contents_window_delegate_;

  // We keep a reference on the HWNDView so we can properly detach the tab
  // contents when detaching.
  ChromeViews::HWNDView* contents_container_;

  // true if this window is really a constrained dialog. This is set by
  // InitAsDialog().
  bool is_dialog_;

  // Current "anchor point", the lower right point at which we render
  // the constrained title bar.
  gfx::Point anchor_point_;

  // The 0,1 percentage representing what amount of a titlebar of a
  // suppressed popup window should be visible. Used to animate those
  // titlebars in.
  double titlebar_visibility_;

  // The animation class which animates constrained windows onto the page.
  scoped_ptr<ConstrainedWindowAnimation> animation_;

  // Current display rectangle (relative to owner_'s visible area).
  gfx::Rect current_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowImpl);
};

#endif  // #ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_
