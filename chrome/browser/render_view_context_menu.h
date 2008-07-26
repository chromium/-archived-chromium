// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  std::vector<std::wstring> misspelled_word_suggestions_;
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_H__
