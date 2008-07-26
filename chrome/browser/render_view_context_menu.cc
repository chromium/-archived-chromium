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

#include "chrome/browser/render_view_context_menu.h"

#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/template_url_model.h"
#include "webkit/glue/context_node_types.h"

#include "generated_resources.h"

RenderViewContextMenu::RenderViewContextMenu(
    Menu::Delegate* delegate,
    HWND owner,
    ContextNode::Type type,
    const std::vector<std::wstring>& misspelled_word_suggestions,
    Profile* profile)
    : Menu(delegate, Menu::TOPLEFT, owner),
      misspelled_word_suggestions_(misspelled_word_suggestions),
      profile_(profile) {
  InitMenu(type);
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

void RenderViewContextMenu::InitMenu(ContextNode::Type type) {
  switch (type) {
   case ContextNode::PAGE:
    AppendPageItems();
    break;
   case ContextNode::FRAME:
    AppendFrameItems();
    break;
   case ContextNode::LINK:
    AppendLinkItems();
    break;
   case ContextNode::IMAGE:
    AppendImageItems();
    break;
   case ContextNode::IMAGE_LINK:
    AppendLinkItems();
    AppendSeparator();
    AppendImageItems();
    break;
   case ContextNode::SELECTION:
    AppendSelectionItems();
    break;
   case ContextNode::EDITABLE:
    AppendEditableItems();
    break;
   default:
    NOTREACHED() << "Unknown ContextNode::Type";
  }
  AppendSeparator();
  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendDeveloperItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendLinkItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVELINKAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendImageItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendPageItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PRINT);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendSelectionItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
  DCHECK(profile_);
  if (profile_->GetTemplateURLModel()->GetDefaultSearchProvider() != NULL)
    AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR);
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < misspelled_word_suggestions_.size() &&
       IDC_USESPELLCHECKSUGGESTION_0 + i <= IDC_USESPELLCHECKSUGGESTION_LAST;
       i ++) {
    AppendMenuItemWithLabel(IDC_USESPELLCHECKSUGGESTION_0 + static_cast<int>(i),
                            misspelled_word_suggestions_[i]);
  }
  if (misspelled_word_suggestions_.size() > 0)
    AppendSeparator();

  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_UNDO);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_REDO);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_CUT);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PASTE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_DELETE);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SELECTALL);
}
