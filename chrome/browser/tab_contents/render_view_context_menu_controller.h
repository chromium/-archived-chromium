// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_

#include "build/build_config.h"

#include "base/string16.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
// TODO(port): port this file.
#include "chrome/views/menu.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#include "chrome/views/accelerator.h"
#endif

class WebContents;

class RenderViewContextMenuController : public Menu::Delegate {
 public:
  RenderViewContextMenuController(WebContents* source_web_contents,
                                  const ContextMenuParams& params);
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
  void WriteTextToClipboard(const string16& text);
  void WriteURLToClipboard(const GURL& url);

  bool IsDevCommandEnabled(int id) const;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewContextMenuController);

 private:
  WebContents* source_web_contents_;
  ContextMenuParams params_;
  StringPrefMember dictionary_language_;
  int current_dictionary_language_index_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H_
