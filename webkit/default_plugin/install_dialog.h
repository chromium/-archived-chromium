// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__
#define WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__

#include <atlbase.h>
#include <atlwin.h>
#include <string>

#include "webkit/default_plugin/default_plugin_resources.h"

class PluginInstallerImpl;

// Displays the plugin installation dialog containing information
// about the mime type of the plugin being downloaded, the URL
// where it would be downloaded from, etc.
class PluginInstallDialog : public CDialogImpl<PluginInstallDialog> {
 public:
  PluginInstallDialog()
      : plugin_impl_(NULL) {
  }
  ~PluginInstallDialog() {}

  enum {IDD = IDD_DEFAULT_PLUGIN_INSTALL_DIALOG};

  BEGIN_MSG_MAP(PluginInstallDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDB_GET_THE_PLUGIN, OnGetPlugin)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  bool Initialize(PluginInstallerImpl* plugin_impl,
                  const std::wstring& plugin_name);

  // Implemented to ensure that we handle RTL layouts correctly.
  HWND Create(HWND parent_window, LPARAM init_param);

 protected:
  LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnGetPlugin(WORD notify_code, WORD id, HWND wnd_ctl, BOOL &handled);
  LRESULT OnCancel(WORD notify_code, WORD id, HWND wnd_ctl, BOOL &handled);

  // Determines whether the UI layout is right-to-left.
  bool IsRTLLayout() const;

  // Wraps the string with Unicode directionality characters in order to make
  // sure BiDi text is rendered correctly when the UI layout is right-to-left.
  void AdjustTextDirectionality(std::wstring* text) const;

  PluginInstallerImpl* plugin_impl_;
  std::wstring plugin_name_;
};

#endif  // WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__

