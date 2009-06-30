// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_WIN_H_
#define CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_WIN_H_

#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "views/window/window_win.h"

class ConstrainedTabContentsWindowDelegate;
class ConstrainedWindowAnimation;
class ConstrainedWindowFrameView;
namespace views {
class WindowDelegate;
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin
//
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame.
//
class ConstrainedWindowWin : public ConstrainedWindow,
                             public views::WindowWin {
 public:
  virtual ~ConstrainedWindowWin();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  // Overridden from views::Window:
  virtual views::NonClientFrameView* CreateFrameViewForWindow();

  // Overridden from ConstrainedWindow:
  virtual void CloseConstrainedWindow();
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
  ConstrainedWindowWin(TabContents* owner,
                       views::WindowDelegate* window_delegate);

  // Moves this window to the front of the Z-order and registers us with the
  // focus manager.
  void ActivateConstrainedWindow();

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // Current "anchor point", the lower right point at which we render
  // the constrained title bar.
  gfx::Point anchor_point_;

  // Current display rectangle (relative to owner_'s visible area).
  gfx::Rect current_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_WIN_H_
