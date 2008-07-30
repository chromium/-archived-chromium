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

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__

#include "chrome/views/dialog_delegate.h"
#include "googleurl/src/gurl.h"

class MessageBoxView;
class TabContents;

class ExternalProtocolDialog : public ChromeViews::DialogDelegate {
 public:
  // Creates and runs a External Protocol dialog box.
  // |url| - The url of the request.
  // |render_process_host_id| and |routing_id| are used by
  // tab_util::GetTabContentsByID to aquire the tab contents associated with
  // this dialog.
  static void RunExternalProtocolDialog(const GURL& url,
                                        int render_process_host_id,
                                        int routing_id);
  virtual ~ExternalProtocolDialog();

  // ChromeViews::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual int GetDefaultDialogButton() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Accept();
  virtual ChromeViews::View* GetContentsView();

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool IsModal() const { return false; }

 private:
  // RunExternalProtocolDialog calls this private constructor.
  ExternalProtocolDialog(TabContents* tab_contents,
                         const GURL& url);

  // Returns the path of the application to be launched given the protocol
  // of the requested url. Returns an empty string on failure.
  std::wstring GetApplicationForProtocol();

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // The associated TabContents.
  TabContents* tab_contents_;

  // URL of the external protocol request.
  GURL url_;

  DISALLOW_EVIL_CONSTRUCTORS(ExternalProtocolDialog);
};

#endif // CHROME_BROWSER_EXTERNAL_PROTOCOL_DIALOG_H__
