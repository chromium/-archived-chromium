// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_HUNG_RENDERER_VIEW_H__
#define CHROME_BROWSER_VIEWS_HUNG_RENDERER_VIEW_H__

#include "base/basictypes.h"

class HungRendererWarningView;
namespace views {
class Window;
}
class TabContents;

class HungRendererWarning {
 public:
  static void ShowForTabContents(TabContents* contents);
  static void HideForTabContents(TabContents* contents);

 private:
  friend HungRendererWarningView;

  // We only support showing one of these at a time per app.
  static HungRendererWarningView* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(HungRendererWarning);
};


#endif  // CHROME_BROWSER_VIEWS_HUNG_RENDERER_VIEW_H__
