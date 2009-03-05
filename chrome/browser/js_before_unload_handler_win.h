// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_WIN_H_
#define CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_WIN_H_

#include "chrome/browser/jsmessage_box_handler_win.h"

class WebContents;

class JavascriptBeforeUnloadHandler : public JavascriptMessageBoxHandler {
 public:
  // Cross-platform code should use RunBeforeUnloadDialog.
  JavascriptBeforeUnloadHandler(WebContents* web_contents,
                                const GURL& frame_url,
                                const std::wstring& message_text,
                                IPC::Message* reply_msg);
  virtual ~JavascriptBeforeUnloadHandler() {}

  // views::DialogDelegate Methods:
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavascriptBeforeUnloadHandler);
};

#endif // CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_WIN_H_
