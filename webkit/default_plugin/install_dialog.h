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

#ifndef WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__
#define WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__

#include <atlbase.h>
#include <atlwin.h>
#include <string>

#include "webkit/glue/webkit_resources.h"

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
