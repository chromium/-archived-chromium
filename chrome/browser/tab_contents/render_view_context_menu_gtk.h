// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_

#include <map>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"

class ContextMenuParams;
class RenderWidgetHostView;

// TODO(port): we need accelerator support for this class.
class RenderViewContextMenuGtk : public RenderViewContextMenu,
                                 public MenuGtk::Delegate {
 public:
  RenderViewContextMenuGtk(TabContents* web_contents,
                           const ContextMenuParams& params,
                           uint32_t triggering_event_time);

  ~RenderViewContextMenuGtk();

  // Show the menu at the current cursor location.
  void Popup();

  // Menu::Delegate implementation ---------------------------------------------
  virtual bool IsCommandEnabled(int id) const;
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);
  virtual std::string GetLabel(int id) const;
  virtual void StoppedShowing();

 protected:
  // RenderViewContextMenu implementation --------------------------------------
  virtual void DoInit();
  virtual void AppendMenuItem(int id);
  virtual void AppendMenuItem(int id, const string16& label);
  virtual void AppendRadioMenuItem(int id, const string16& label);
  virtual void AppendCheckboxMenuItem(int id, const string16& label);
  virtual void AppendSeparator();
  virtual void StartSubMenu(int id, const string16& label);
  virtual void FinishSubMenu();
  virtual void DidWriteURLToClipboard(const std::string& url);

 private:
  void AppendItem(int id, const string16& label, MenuItemType type);
  static void DoneMakingMenu(std::vector<MenuCreateMaterial>* menu);

  scoped_ptr<MenuGtk> gtk_menu_;
  std::map<int, std::string> label_map_;
  std::vector<MenuCreateMaterial> menu_;
  std::vector<MenuCreateMaterial> submenu_;
  bool making_submenu_;
  uint32_t triggering_event_time_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_
