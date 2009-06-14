// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_EXTERNAL_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_EXTERNAL_WIN_H_

#include <vector>

#include "chrome/browser/views/tab_contents/render_view_context_menu_win.h"

// This class provides a facility for an external host to customize the context
// menu displayed in Chrome.
class RenderViewContextMenuExternalWin : public RenderViewContextMenuWin {
 public:
  RenderViewContextMenuExternalWin(TabContents* tab_contents,
                                   const ContextMenuParams& params,
                                   const std::vector<int> disabled_menu_ids);

  ~RenderViewContextMenuExternalWin() {}

 protected:
  // RenderViewContextMenuWin overrides --------------------------------------
  virtual void AppendMenuItem(int id);

  // RenderViewContextMenu override
  virtual void DoInit();

 private:
  // Contains the list of context menu ids to be disabled.
  std::vector<int> disabled_menu_ids_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_EXTERNAL_WIN_H_
