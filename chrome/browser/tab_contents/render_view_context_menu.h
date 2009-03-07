// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_

#include "base/string16.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/window_open_disposition.h"

class Profile;
class WebContents;

class RenderViewContextMenu {
 public:
  RenderViewContextMenu(
      WebContents* web_contents,
      const ContextMenuParams& params);

  virtual ~RenderViewContextMenu();

 protected:
  void InitMenu(ContextNode node);

  // Functions to be implemented by platform-specific subclasses ---------------

  // Append a normal menu item, taking the name from the id.
  virtual void AppendMenuItem(int id) = 0;

  // Append a normal menu item, using |label| for the name.
  virtual void AppendMenuItem(int id, const std::wstring& label) = 0;

  // Append a radio menu item.
  virtual void AppendRadioMenuItem(int id, const std::wstring& label) = 0;

  // Append a checkbox menu item.
  virtual void AppendCheckboxMenuItem(int id, const std::wstring& label) = 0;

  // Append a separator.
  virtual void AppendSeparator() = 0;

  // Start creating a submenu. Any calls to Append*() between calls to
  // StartSubMenu() and FinishSubMenu() will apply to the submenu rather than
  // the main menu we are building. We only support at most single-depth
  // submenus, so calls to StartSubMenu() while we are already building a
  // submenu will be ignored.
  virtual void StartSubMenu(int id, const std::wstring& label) = 0;

  // Finish creating the submenu and attach it to the main menu.
  virtual void FinishSubMenu() = 0;

  // Delegate functions --------------------------------------------------------

  bool IsItemCommandEnabled(int id) const;
  bool ItemIsChecked(int id) const;
  void ExecuteItemCommand(int id);

 private:
  void AppendDeveloperItems();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendCopyItem();
  void AppendEditableItems();
  void AppendSearchProvider();

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
  void WriteTextToClipboard(const string16& text);
  void WriteURLToClipboard(const GURL& url);

  bool IsDevCommandEnabled(int id) const;

  ContextMenuParams params_;
  WebContents* source_web_contents_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
