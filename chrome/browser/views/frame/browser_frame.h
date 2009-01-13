// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_

class BrowserView;
namespace views {
class Window;
}
namespace gfx {
class Rect;
}
class TabStrip;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame
//
//  BrowserFrame is an interface that represents a top level browser window
//  frame. Implementations of this interface exist to supply the browser window
//  for specific environments, e.g. Vista with Aero Glass enabled.
//
class BrowserFrame {
 public:
  // TODO(beng): We should _not_ have to expose this method here... it's only
  //             because BrowserView needs it to implement BrowserWindow
  //             because we're doing popup setup in browser.cc when we
  //             shouldn't be...
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) = 0;

  // Sizes the frame assuming the contents view's bounds are as specified.
  virtual void SizeToContents(const gfx::Rect& contents_bounds) = 0;

  // Retrieve the bounds for the specified |tabstrip|, in window coordinates.
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const = 0;

  // Updates the current frame of the Throbber animation, if applicable.
  // |running| is whether or not the throbber should be running.
  virtual void UpdateThrobber(bool running) = 0;

  // Returns the views::Window associated with this frame.
  virtual views::Window* GetWindow() = 0;

  enum FrameType {
    FRAMETYPE_OPAQUE,
    FRAMETYPE_AERO_GLASS
  };

  // Returns the FrameType that should be constructed given the current system
  // settings.
  static FrameType GetActiveFrameType();

  // Creates a BrowserFrame instance for the specified FrameType and
  // BrowserView.
  static BrowserFrame* CreateForBrowserView(FrameType type,
                                            BrowserView* browser_view);

};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_

