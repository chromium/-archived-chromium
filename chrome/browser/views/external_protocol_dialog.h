// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__

#include "chrome/views/window/dialog_delegate.h"
#include "googleurl/src/gurl.h"

class MessageBoxView;
class TabContents;

class ExternalProtocolDialog : public views::DialogDelegate {
 public:
  // Creates and runs a External Protocol dialog box.
  // |url| - The url of the request.
  // |command| - the command that ShellExecute will run.
  // |render_process_host_id| and |routing_id| are used by
  // tab_util::GetWebContentsByID to aquire the tab contents associated with
  // this dialog.
  // NOTE: There is a race between the Time of Check and the Time Of Use for
  //       the command line. Since the caller (web page) does not have access
  //       to change the command line by itself, we do not do anything special
  //       to protect against this scenario.
  static void RunExternalProtocolDialog(const GURL& url,
                                        const std::wstring& command,
                                        int render_process_host_id,
                                        int routing_id);

  // Returns the path of the application to be launched given the protocol
  // of the requested url. Returns an empty string on failure.
  static std::wstring GetApplicationForProtocol(const GURL& url);

  virtual ~ExternalProtocolDialog();

  // views::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual int GetDefaultDialogButton() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void DeleteDelegate();
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // views::WindowDelegate Methods:
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool IsModal() const { return false; }

 private:
  // RunExternalProtocolDialog calls this private constructor.
  ExternalProtocolDialog(TabContents* tab_contents,
                         const GURL& url,
                         const std::wstring& command);

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // The associated TabContents.
  TabContents* tab_contents_;

  // URL of the external protocol request.
  GURL url_;

  DISALLOW_EVIL_CONSTRUCTORS(ExternalProtocolDialog);
};

#endif // CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__
