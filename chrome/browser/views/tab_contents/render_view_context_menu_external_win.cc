// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_contents/render_view_context_menu_external_win.h"

#include <algorithm>

RenderViewContextMenuExternalWin::RenderViewContextMenuExternalWin(
    TabContents* tab_contents,
    const ContextMenuParams& params,
    const std::vector<int> disabled_ids)
    : RenderViewContextMenuWin(tab_contents, params),
      disabled_menu_ids_(disabled_ids) {
}

void RenderViewContextMenuExternalWin::AppendMenuItem(int id) {
  std::vector<int>::iterator found =
      std::find(disabled_menu_ids_.begin(), disabled_menu_ids_.end(), id);

  if (found == disabled_menu_ids_.end()) {
    RenderViewContextMenuWin::AppendMenuItem(id);
  }
}

void RenderViewContextMenuExternalWin::DoInit() {
  RenderViewContextMenuWin::DoInit();
  // The external tab container needs to be notified by command
  // and not by index. So we are turning off the MNS_NOTIFYBYPOS
  // style.
  HMENU menu = GetMenuHandle();
  DCHECK(menu != NULL);

  MENUINFO mi = {0};
  mi.cbSize = sizeof(mi);
  mi.fMask = MIM_STYLE | MIM_MENUDATA;
  mi.dwMenuData = reinterpret_cast<ULONG_PTR>(this);
  SetMenuInfo(menu, &mi);
}
