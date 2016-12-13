// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_

#include "views/window/non_client_view.h"

class TabStripWrapper;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  BrowserNonClientFrameView() : NonClientFrameView() {}
  virtual ~BrowserNonClientFrameView() {}

  // Returns the bounds within which the TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(TabStripWrapper* tabstrip) const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_
