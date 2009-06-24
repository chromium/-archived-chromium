// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/tab_contents/render_view_context_menu.h"

@class ContextMenuTarget;

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.

class RenderViewContextMenuMac : public RenderViewContextMenu {
 public:
  RenderViewContextMenuMac(TabContents* web_contents,
                           const ContextMenuParams& params,
                           NSView* parent_view);
  virtual ~RenderViewContextMenuMac();

  // Elevate to public so that the obj-c target can call it.
  void ExecuteCommand(int command_id) { ExecuteItemCommand(command_id); }

 protected:
  // RenderViewContextMenu implementation-
  virtual void DoInit();
  virtual void AppendMenuItem(int id);
  virtual void AppendMenuItem(int id, const string16& label);
  virtual void AppendRadioMenuItem(int id, const string16& label);
  virtual void AppendCheckboxMenuItem(int id, const string16& label);
  virtual void AppendSeparator();
  virtual void StartSubMenu(int id, const string16& label);
  virtual void FinishSubMenu();

  // Do things like remove the windows accelerators.
  static NSString* PrepareLabelForDisplay(const string16& label);

 private:
  NSMenu* menu_;
  NSMenu* insert_menu_;  // weak, where new items are inserted (usually
                         // |menu_| unless there's a submenu in progress).
  ContextMenuTarget* target_;  // obj-c target for menu actions
  NSView* parent_view_;  // parent view
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
