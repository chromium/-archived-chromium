// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_GTK_BROWSER_FRAME_GTK_H_
#define CHROME_BROWSER_VIEWS_FRAME_GTK_BROWSER_FRAME_GTK_H_

#include "base/basictypes.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "views/window/window_gtk.h"

class BrowserNonClientFrameView;
class BrowserRootView;

class BrowserFrameGtk : public BrowserFrame,
                        public views::WindowGtk {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameGtk(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameGtk();

  // This initialization function must be called after construction, it is
  // separate to avoid recursive calling of the frame from its constructor.
  void Init();

  // BrowserFrame implementation.
  virtual views::Window* GetWindow();
  virtual void TabStripCreated(TabStripWrapper* tabstrip);
  virtual int GetMinimizeButtonOffset() const;
  virtual gfx::Rect GetBoundsForTabStrip(TabStripWrapper* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual void ContinueDraggingDetachedTab();
  virtual ThemeProvider* GetThemeProviderForFrame() const;

  // Overridden from views::Widget.
  virtual ThemeProvider* GetThemeProvider() const;
  virtual ThemeProvider* GetDefaultThemeProvider() const;

 protected:
  // WidgetGtk overrides.
  virtual views::RootView* CreateRootView();

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  // An unowning reference to the root view associated with the window. We save
  // a copy as a BrowserRootView to avoid evil casting later, when we need to
  // call functions that only exist on BrowserRootView (versus RootView).
  BrowserRootView* root_view_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameGtk);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_GTK_BROWSER_FRAME_GTK_H_
