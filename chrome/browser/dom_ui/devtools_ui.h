// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DEVTOOLS_UI_H_
#define CHROME_BROWSER_DOM_UI_DEVTOOLS_UI_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class DevToolsUI : public DOMUI {
 public:
  explicit DevToolsUI(TabContents* contents);

  // DOMUI overrides.
  virtual void RenderViewCreated(RenderViewHost* render_view_host);

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsUI);
};

#endif  // CHROME_BROWSER_DOM_UI_DEVTOOLS_UI_H_
