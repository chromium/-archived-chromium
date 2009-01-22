// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_

#include "chrome/common/pref_member.h"
#include "chrome/views/menu.h"
#include "chrome/common/render_messages.h"

class WebContents;

class RenderViewContextMenuController : public Menu::Delegate {
 public:
  RenderViewContextMenuController(WebContents* source_web_contents,
                                  const ViewHostMsg_ContextMenu_Params& params);
  virtual ~RenderViewContextMenuController();

  // Overridden from Menu::Delegate
  virtual std::wstring GetLabel(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);
  virtual bool GetAcceleratorInfo(int id, views::Accelerator* accel);

 private:
  // Opens the specified URL string in a new tab.  If |in_current_window| is
  // false, a new window is created to hold the new tab.
  void OpenURL(const GURL& url,
               WindowOpenDisposition disposition,
               PageTransition::Type transition);

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Launch the inspector targeting a point in the RenderView
  void Inspect(int x, int y);

  // Writes the specified text/url to the system clipboard
  void WriteTextToClipboard(const std::wstring& text);
  void WriteURLToClipboard(const GURL& url);

  bool IsDevCommandEnabled(int id) const;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewContextMenuController);

 private:
  WebContents* source_web_contents_;
  ViewHostMsg_ContextMenu_Params params_;
  StringPrefMember dictionary_language_;
  int current_dictionary_language_index_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_

