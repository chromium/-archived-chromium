// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_
#define CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_

#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/views/window_win.h"

class ConstrainedTabContentsWindowDelegate;
class ConstrainedWindowAnimation;
class ConstrainedWindowFrameView;
namespace views {
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
                              public views::WindowWin {
 public:
  virtual ~ConstrainedWindowImpl();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  // Overridden from views::Window:
  virtual views::NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateWindowTitle();

  // Overridden from ConstrainedWindow:
  virtual void CloseConstrainedWindow();
  virtual void ActivateConstrainedWindow();
  virtual void RepositionConstrainedWindowTo(const gfx::Point& anchor_point) {}
  virtual void WasHidden();
  virtual void DidBecomeSelected();
  virtual std::wstring GetWindowTitle() const;
  virtual const gfx::Rect& GetCurrentBounds() const;

 protected:
  // Windows message handlers:
  virtual void OnDestroy();
  virtual void OnFinalMessage(HWND window);
  virtual LRESULT OnMouseActivate(HWND window, UINT hittest_code, UINT message);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);

 private:
  friend class ConstrainedWindow;

  // Use the static factory methods on ConstrainedWindow to construct a
  // ConstrainedWindow.
  ConstrainedWindowImpl(TabContents* owner,
                        views::WindowDelegate* window_delegate);
  void Init();

  // Initialize the Constrained Window as a Constrained Dialog containing a
  // views::View client area.
  void InitAsDialog(const gfx::Rect& initial_bounds);

  // Updates the portions of the UI as specified in |changed_flags|.
  void UpdateUI(unsigned int changed_flags);

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // True if focus should not be restored to whatever view was focused last
  // when this window is destroyed.
  bool focus_restoration_disabled_;

  // true if this window is really a constrained dialog. This is set by
  // InitAsDialog().
  bool is_dialog_;

  // Current "anchor point", the lower right point at which we render
  // the constrained title bar.
  gfx::Point anchor_point_;

  // Current display rectangle (relative to owner_'s visible area).
  gfx::Rect current_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowImpl);
};

#endif  // #ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_IMPL_H_
