// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "webkit/glue/context_menu.h"

// TODO(port): Port this file.
#if defined(OS_WIN)
#include "chrome/views/menu.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Profile;

class RenderViewContextMenu : public Menu {
 public:
  RenderViewContextMenu(
      Menu::Delegate* delegate,
      gfx::NativeWindow owner,
      ContextNode node,
      const std::wstring& misspelled_word,
      const std::vector<std::wstring>& misspelled_word_suggestions,
      Profile* profile);

  virtual ~RenderViewContextMenu();

 private:
  void InitMenu(ContextNode node);
  void AppendDeveloperItems();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendCopyItem();
  void AppendEditableItems();
  void AppendSearchProvider();

  std::wstring misspelled_word_;
  std::vector<std::wstring> misspelled_word_suggestions_;
  Profile* profile_;
  Menu* spellchecker_sub_menu_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_

