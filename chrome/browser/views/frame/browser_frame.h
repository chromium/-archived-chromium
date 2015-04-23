// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_

#include "build/build_config.h"
#include "views/window/non_client_view.h"

class BrowserView;
class Profile;
class TabStripWrapper;
class ThemeProvider;

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Window;

#if defined(OS_WIN)
class WindowWin;
#endif
}  // namespace views

// This is a virtual interface that allows system specific browser frames.
class BrowserFrame {
 public:
  virtual ~BrowserFrame() {}

  // Creates the appropriate BrowserFrame for this platform. The returned
  // object is owned by the caller.
  static BrowserFrame* Create(BrowserView* browser_view, Profile* profile);

  // Returns the Window associated with this frame. Guraranteed non-NULL after
  // construction.
  virtual views::Window* GetWindow() = 0;

  // Notification that the tab strip has been created. This should let the
  // BrowserRootView know about it so it can enable drag and drop.
  virtual void TabStripCreated(TabStripWrapper* tabstrip) = 0;

  // Determine the distance of the left edge of the minimize button from the
  // left edge of the window. Used in our Non-Client View's Layout.
  virtual int GetMinimizeButtonOffset() const = 0;

  // Retrieves the bounds, in non-client view coordinates for the specified
  // TabStrip.
  virtual gfx::Rect GetBoundsForTabStrip(TabStripWrapper* tabstrip) const = 0;

  // Tells the frame to update the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Tells the frame to continue a drag detached tab operation.
  virtual void ContinueDraggingDetachedTab() = 0;

  // Returns the theme provider for this frame.
  virtual ThemeProvider* GetThemeProviderForFrame() const = 0;
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_
