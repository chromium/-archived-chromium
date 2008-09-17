// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_H__
#define CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_H__

#include "chrome/views/menu.h"
#include "webkit/glue/context_node_types.h"

class Profile;

class RenderViewContextMenu : public Menu {
 public:
  RenderViewContextMenu(
      Menu::Delegate* delegate,
      HWND owner,
      ContextNode::Type type,
      const std::wstring& misspelled_word,
      const std::vector<std::wstring>& misspelled_word_suggestions,
      Profile* profile);

  virtual ~RenderViewContextMenu();

 private:
  void InitMenu(ContextNode::Type type);
  void AppendDeveloperItems();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendSelectionItems();
  void AppendEditableItems();

  std::wstring misspelled_word_;
  std::vector<std::wstring> misspelled_word_suggestions_;
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_H__

