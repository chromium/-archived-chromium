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

#ifndef CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__
#define CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__

#include "chrome/browser/jsmessage_box_handler.h"

class JavascriptBeforeUnloadHandler : public JavascriptMessageBoxHandler {
 public:
  // This will display a modal dialog box with a header and footer asking the
  // the user if they wish to navigate away from a page, with additional text
  // |message_text| between the header and footer. The users response is
  // returned to the renderer using |reply_msg|.
  static void RunBeforeUnloadDialog(WebContents* web_contents,
                                    const std::wstring& message_text,
                                    IPC::Message* reply_msg);
  virtual ~JavascriptBeforeUnloadHandler() {}

  // ChromeViews::DialogDelegate Methods:
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;

 private:
  // Called from RunBeforeUnloadDialog. Calls JavascriptMessageBoxHandler's
  // constructor.
  JavascriptBeforeUnloadHandler(WebContents* web_contents,
                                const std::wstring& message_text,
                                IPC::Message* reply_msg);

  DISALLOW_EVIL_CONSTRUCTORS(JavascriptBeforeUnloadHandler);
};

#endif // CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__
